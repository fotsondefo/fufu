#include "depch.h"
#include "Project/Scene/Scene.h"
#include "Renderer/Renderer.h"
#include "Project/Components.h"
#include "Project/Assets/AssetManager.h"
#include "Renderer/GroomGenerator.h"
#include "Application/Application.h"

namespace Fufu 
{

	// ----------------------------------------------------------------
	// Compute shader � path tracing complet
	// GI + shadows + reflections + r�fraction
	// ----------------------------------------------------------------

	static const char* s_ComputeSrc = R"(
#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D u_Output;
layout(rgba32f, binding = 1) uniform image2D u_Accum;

struct Triangle {
    vec4 v0, v1, v2;
    vec4 n0, n1, n2;
    vec2 uv0, uv1, uv2;
    int  materialIndex;
    float _pad;
};

struct Material {
    vec4  albedo;
    float metallic;
    float roughness;
    float emissive;
    float ior;
    int   albedoTexIdx;
    float _pad[3];
};

struct BVHNode {
    vec4 boundsMin;
    vec4 boundsMax;
    int  left;
    int  right;
    int  firstTri;   // feuille BLAS : triangle. feuille TLAS : instance.
    int  triCount;
};

struct Instance {
    mat4 transform;
    mat4 invTransform;
    int  materialIndex;
    int  blasNodeOffset;
    int  blasTriOffset;
    int  _pad;
};

// Directional (type 0) : positionOrDirection.xyz = direction vers la lumière
// (normalisé), radius = rayon angulaire (rad).
// Point (type 1) : positionOrDirection.xyz = position monde, radius = rayon
// physique de la source (atténuation 1/distance² calculée dans tracePath).
struct Light {
    vec4  positionOrDirection;
    vec4  color; // rgb = couleur, a = intensité
    float radius;
    int   type;
    float _pad[2];
};

layout(std430, binding = 2) readonly buffer TriangleBuffer { Triangle triangles[]; };   // BLAS, espace local
layout(std430, binding = 3) readonly buffer MaterialBuffer { Material materials[]; };
layout(std430, binding = 6) readonly buffer BLASNodeBuffer { BVHNode blasNodes[]; };
layout(std430, binding = 7) readonly buffer InstanceBuffer { Instance instances[]; };
layout(std430, binding = 8) readonly buffer TLASBuffer     { BVHNode tlasNodes[]; };
layout(std430, binding = 9) readonly buffer LightBuffer    { Light lights[]; };

layout(std140, binding = 4) uniform CameraBlock {
    vec4  camPosition;
    vec4  camForward;
    vec4  camRight;
    vec4  camUp;
    float camFov;
    float camAspect;
    float camNear;
    float _pad;
};

layout(std140, binding = 5) uniform FrameBlock {
    int   frameIndex;
    int   maxBounces;
    int   samplesPerPixel;
    float exposure;
    int   width;
    int   height;
    int   triangleCount;
    int   materialCount;
    int   lightCount;
    int   technique; // 0 = PathTracing, 1 = RayTracing
    int   aaMode;         // 0=None, 1=SSAA, 2=TAA, 3=FXAA
    int   taaFrameIndex;  // dédié au TAA : incrémente à chaque frame, indépendamment du RenderMode
    float taaBlendFactor;
};

// ---- RNG (PCG hash) ----
uint pcg(inout uint state) {
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
float rand(inout uint seed) {
    return float(pcg(seed)) / 4294967296.0;
}

// ---- G�om�trie ----
struct Ray { vec3 origin; vec3 dir; };

struct HitInfo {
    float t;
    vec3  normal;
    vec2  uv;
    int   materialIndex;
    bool  hit;
};

HitInfo intersectTriangle(Ray ray, Triangle tri) {
    HitInfo info;
    info.hit = false;

    vec3 v0 = tri.v0.xyz, v1 = tri.v1.xyz, v2 = tri.v2.xyz;
    vec3 e1 = v1 - v0, e2 = v2 - v0;
    vec3 h  = cross(ray.dir, e2);
    float a = dot(e1, h);
    if (abs(a) < 1e-6) return info;

    float f = 1.0 / a;
    vec3  s = ray.origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0) return info;

    vec3  q = cross(s, e1);
    float v = f * dot(ray.dir, q);
    if (v < 0.0 || u + v > 1.0) return info;

    float t = f * dot(e2, q);
    if (t < 1e-4) return info;

    float w = 1.0 - u - v;
    info.hit           = true;
    info.t             = t;
    info.normal        = normalize(w * tri.n0.xyz + u * tri.n1.xyz + v * tri.n2.xyz);
    info.uv            = w * tri.uv0 + u * tri.uv1 + v * tri.uv2;
    info.materialIndex = tri.materialIndex;
    return info;
}

// Slab test AABB. tNear renseigné même en cas de non-hit (utilisé pour l'élagage).
bool intersectAABB(Ray ray, vec3 bmin, vec3 bmax, out float tNear) {
    vec3 invDir = 1.0 / ray.dir;
    vec3 t0 = (bmin - ray.origin) * invDir;
    vec3 t1 = (bmax - ray.origin) * invDir;
    vec3 tsmall = min(t0, t1);
    vec3 tbig   = max(t0, t1);
    float tEnter = max(max(tsmall.x, tsmall.y), tsmall.z);
    float tExit  = min(min(tbig.x, tbig.y), tbig.z);
    tNear = tEnter;
    return tExit >= max(tEnter, 0.0);
}

// Parcours du BLAS d'UNE instance (espace local à cette instance). Les
// indices left/right/firstTri stockés dans les nœuds sont RELATIFS au bloc du
// BLAS (0-based) : on ajoute nodeOffset seulement au moment de lire le nœud,
// pour que la même géométrie (même BLAS) puisse être traversée par n'importe
// quelle instance qui la référence — c'est ça, l'instancing.
HitInfo intersectBLAS(Ray ray, int nodeOffset, int triOffset) {
    HitInfo closest;
    closest.hit = false;
    closest.t   = 1e30;

    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // racine du BLAS (index local)

    while (stackPtr > 0) {
        int localIdx = stack[--stackPtr];
        BVHNode node = blasNodes[nodeOffset + localIdx];

        float tNear;
        if (!intersectAABB(ray, node.boundsMin.xyz, node.boundsMax.xyz, tNear)) continue;
        if (tNear > closest.t) continue;

        if (node.triCount > 0) {
            for (int i = 0; i < node.triCount; ++i) {
                HitInfo h = intersectTriangle(ray, triangles[triOffset + node.firstTri + i]);
                if (h.hit && h.t < closest.t)
                    closest = h;
            }
        } else {
            stack[stackPtr++] = node.left;
            stack[stackPtr++] = node.right;
        }
    }

    return closest;
}

// Parcours du TLAS (espace monde) : pour chaque instance candidate, transforme
// le rayon en espace local avant de descendre dans son BLAS, puis retransforme
// le résultat (distance + normale) en espace monde.
HitInfo intersectScene(Ray ray) {
    HitInfo closest;
    closest.hit = false;
    closest.t   = 1e30;

    // triangleCount réutilisé comme "nombre d'instances" côté C++ : sert
    // juste à éviter de descendre dans un TLAS vide (nœud racine sentinelle).
    if (triangleCount <= 0) return closest;

    int stack[64];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // racine du TLAS

    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        BVHNode node = tlasNodes[nodeIdx];

        float tNear;
        if (!intersectAABB(ray, node.boundsMin.xyz, node.boundsMax.xyz, tNear)) continue;
        if (tNear > closest.t) continue;

        if (node.triCount > 0) {
            // Feuille du TLAS : firstTri/triCount indexent des INSTANCES ici
            for (int i = 0; i < node.triCount; ++i) {
                Instance inst = instances[node.firstTri + i];

                vec3 localOrigin = (inst.invTransform * vec4(ray.origin, 1.0)).xyz;
                vec3 localDirRaw = (inst.invTransform * vec4(ray.dir, 0.0)).xyz;
                float dirLen = length(localDirRaw);
                if (dirLen < 1e-8) continue;

                Ray localRay;
                localRay.origin = localOrigin;
                localRay.dir    = localDirRaw / dirLen;

                HitInfo h = intersectBLAS(localRay, inst.blasNodeOffset, inst.blasTriOffset);
                if (!h.hit) continue;

                // t était mesuré en espace local (rayon normalisé localement) :
                // reconvertir en distance monde avant de comparer.
                float worldT = h.t / dirLen;
                if (worldT < closest.t) {
                    closest = h;
                    closest.t = worldT;
                    closest.materialIndex = inst.materialIndex;
                    mat3 normalMat = mat3(transpose(inst.invTransform));
                    closest.normal = normalize(normalMat * h.normal);
                }
            }
        } else {
            stack[stackPtr++] = node.left;
            stack[stackPtr++] = node.right;
        }
    }

    return closest;
}

// Version "any-hit" du BLAS : s'arrête au tout premier triangle touché AVANT
// maxDist, sans chercher le plus proche. Utilisée pour les tests d'ombre
// (question oui/non — "quelque chose bloque-t-il avant la lumière ?"), qui
// n'ont pas besoin de savoir LEQUEL bloque : sortir dès le premier contact
// évite de continuer à explorer des feuilles pour rien. maxDist est en
// espace LOCAL (voir intersectSceneShadow pour la conversion depuis le monde).
bool intersectBLASAny(Ray ray, int nodeOffset, int triOffset, float maxDist) {
    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = 0;

    while (stackPtr > 0) {
        int localIdx = stack[--stackPtr];
        BVHNode node = blasNodes[nodeOffset + localIdx];

        float tNear;
        if (!intersectAABB(ray, node.boundsMin.xyz, node.boundsMax.xyz, tNear)) continue;
        if (tNear > maxDist) continue;

        if (node.triCount > 0) {
            for (int i = 0; i < node.triCount; ++i) {
                HitInfo h = intersectTriangle(ray, triangles[triOffset + node.firstTri + i]);
                if (h.hit && h.t < maxDist)
                    return true;
            }
        } else {
            stack[stackPtr++] = node.left;
            stack[stackPtr++] = node.right;
        }
    }

    return false;
}

// Version "any-hit" du TLAS, pour les mêmes raisons — utilisée par les rayons
// d'ombre du NEE (voir tracePath) à la place d'intersectScene, nettement
// moins cher qu'une recherche de plus-proche-contact pour un simple booléen.
// maxDistWorld : distance (espace monde) au-delà de laquelle un contact ne
// compte pas comme occlusion — 1e30 pour une directionnelle ("infinie"),
// la distance jusqu'à la source pour une point light (ne pas compter ce qui
// est derrière la lumière comme un obstacle).
bool intersectSceneShadow(Ray ray, float maxDistWorld) {
    if (triangleCount <= 0) return false; // triangleCount réutilisé comme "nombre d'instances"

    int stack[64];
    int stackPtr = 0;
    stack[stackPtr++] = 0;

    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        BVHNode node = tlasNodes[nodeIdx];

        float tNear;
        if (!intersectAABB(ray, node.boundsMin.xyz, node.boundsMax.xyz, tNear)) continue;
        if (tNear > maxDistWorld) continue;

        if (node.triCount > 0) {
            for (int i = 0; i < node.triCount; ++i) {
                Instance inst = instances[node.firstTri + i];

                vec3 localOrigin = (inst.invTransform * vec4(ray.origin, 1.0)).xyz;
                vec3 localDirRaw = (inst.invTransform * vec4(ray.dir, 0.0)).xyz;
                float dirLen = length(localDirRaw);
                if (dirLen < 1e-8) continue;

                Ray localRay;
                localRay.origin = localOrigin;
                localRay.dir    = localDirRaw / dirLen;

                // maxDist est mesuré en espace local (rayon normalisé
                // localement) : même conversion que dans intersectScene.
                float localMaxDist = (maxDistWorld >= 1e29) ? 1e30 : maxDistWorld * dirLen;

                if (intersectBLASAny(localRay, inst.blasNodeOffset, inst.blasTriOffset, localMaxDist))
                    return true;
            }
        } else {
            stack[stackPtr++] = node.left;
            stack[stackPtr++] = node.right;
        }
    }

    return false;
}

// ---- Sampling ----
vec3 cosineHemisphere(vec3 normal, inout uint seed) {
    float u1 = rand(seed), u2 = rand(seed);
    float r   = sqrt(u1);
    float phi = 6.28318530718 * u2;
    vec3  t   = normalize(abs(normal.x) > 0.9 ? vec3(0,1,0) : vec3(1,0,0));
    vec3  bt  = cross(normal, t);
    t         = cross(bt, normal);
    return normalize(r * cos(phi) * t + r * sin(phi) * bt + sqrt(1.0 - u1) * normal);
}

// Échantillonne une direction dans un petit cône autour de `dir` (rayon
// angulaire `angularRadius`) : sert à donner un diamètre apparent aux
// lumières directionnelles, donc des ombres douces plutôt qu'un aliasing net.
vec3 sampleConeDirection(vec3 dir, float angularRadius, inout uint seed) {
    if (angularRadius <= 0.0) return dir;

    float u1 = rand(seed), u2 = rand(seed);
    float cosThetaMax = cos(angularRadius);
    float cosTheta    = mix(cosThetaMax, 1.0, u1);
    float sinTheta    = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi         = 6.28318530718 * u2;

    vec3 t  = normalize(abs(dir.x) > 0.9 ? vec3(0,1,0) : vec3(1,0,0));
    vec3 bt = normalize(cross(dir, t));
    t       = cross(bt, dir);

    return normalize(cosTheta * dir + sinTheta * cos(phi) * t + sinTheta * sin(phi) * bt);
}

// Échantillonne un point uniforme sur la sphère de rayon `radius` autour de
// `center` : donne un diamètre physique aux point lights, donc des ombres
// douces plutôt qu'une source ponctuelle idéale (aliasing net).
vec3 samplePointOnSphere(vec3 center, float radius, inout uint seed) {
    if (radius <= 0.0) return center;

    float u1 = rand(seed), u2 = rand(seed);
    float z   = 1.0 - 2.0 * u1;
    float r   = sqrt(max(0.0, 1.0 - z * z));
    float phi = 6.28318530718 * u2;

    vec3 offset = vec3(r * cos(phi), r * sin(phi), z) * radius;
    return center + offset;
}

vec3 reflect(vec3 v, vec3 n) { return v - 2.0 * dot(v, n) * n; }

vec3 refract(vec3 v, vec3 n, float ior, inout bool tir) {
    float cosI = -dot(v, n);
    float k    = 1.0 - ior * ior * (1.0 - cosI * cosI);
    tir = k < 0.0;
    return tir ? vec3(0) : normalize(ior * v + (ior * cosI - sqrt(k)) * n);
}

// Fresnel Schlick
float fresnel(float cosTheta, float f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

// ---- Path tracing ----
vec3 tracePath(Ray ray, inout uint seed) {
    vec3 radiance    = vec3(0.0);
    vec3 throughput  = vec3(1.0);

    for (int bounce = 0; bounce < maxBounces; ++bounce) {
        HitInfo hit = intersectScene(ray);

        if (!hit.hit) {
            // Environnement simple � ciel gradient
            float t   = 0.5 * (ray.dir.y + 1.0);
            vec3  sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
            radiance += throughput * sky;
            break;
        }

        Material mat = materials[hit.materialIndex];
        vec3 albedo  = mat.albedo.rgb;
        vec3 N       = hit.normal;

        // Point d'intersection réel, calculé AVANT toute réassignation de
        // ray.dir (les branches ci-dessous en avaient besoin après coup, ce
        // qui recalculait un point au hasard le long de la NOUVELLE direction
        // plutôt que le point d'impact — corrigé ici une bonne fois).
        vec3 hitPoint = ray.origin + ray.dir * hit.t;

        // �mission
        if (mat.emissive > 0.0) {
            radiance += throughput * albedo * mat.emissive;
            break;
        }

        float cosTheta = max(dot(-ray.dir, N), 0.0);
        float f0       = mix(0.04, 1.0, mat.metallic);
        float F        = fresnel(cosTheta, f0);

        // Choix stochastique : r�flexion sp�culaire / diffuse / r�fraction
        float specProb = mix(F, 1.0, mat.metallic);
        float refrProb = (mat.albedo.a < 1.0) ? (1.0 - specProb) * (1.0 - mat.albedo.a) : 0.0;
        float r        = rand(seed);

        if (r < specProb) {
            // R�flexion sp�culaire (GGX simplifi� via roughness)
            vec3 roughDir = cosineHemisphere(N, seed);
            vec3 specDir  = reflect(ray.dir, N);
            ray.dir    = normalize(mix(specDir, roughDir, mat.roughness * mat.roughness));
            ray.origin = hitPoint + N * 1e-4;
            throughput *= albedo;
        }
        else if (r < specProb + refrProb) {
            // R�fraction (verre)
            bool tir;
            float eta = dot(ray.dir, N) < 0.0 ? (1.0 / mat.ior) : mat.ior;
            vec3  refractDir = refract(ray.dir, N, eta, tir);
            ray.dir    = tir ? reflect(ray.dir, N) : refractDir;
            ray.origin = hitPoint - N * 1e-4;
            throughput *= albedo;
        }
        else {
            // Diffuse (Lambertian)

            // NEE : échantillonnage direct des lumières. Sans ça, un rayon ne
            // trouve une lumière qu'en la heurtant par hasard au fil d'un
            // rebond — ici on tire un rayon d'ombre direct à chaque rebond
            // diffus, ce qui donne des ombres nettes et une convergence bien
            // plus rapide. Pas de MIS nécessaire : une lumière directionnelle
            // n'a pas de géométrie propre, un rebond indirect ne peut donc
            // jamais la "redécouvrir" par intersection.
            for (int li = 0; li < lightCount; ++li) {
                Light light = lights[li];

                vec3  Ldir;
                float maxDist;       // distance monde au-delà de laquelle un contact ne bloque pas la lumière
                vec3  lightRadiance;

                if (light.type == 0) {
                    // Directionnelle : "infiniment loin", pas d'atténuation par distance.
                    Ldir = sampleConeDirection(light.positionOrDirection.xyz, light.radius, seed);
                    maxDist = 1e30;
                    lightRadiance = light.color.rgb * light.color.a;
                } else {
                    // Point : atténuation en 1/distance², bornée par la distance
                    // jusqu'à la source (ce qui est derrière elle ne bloque pas).
                    vec3 samplePos = samplePointOnSphere(light.positionOrDirection.xyz, light.radius, seed);
                    vec3 toLight   = samplePos - hitPoint;
                    float dist     = length(toLight);
                    Ldir = toLight / max(dist, 1e-6);
                    maxDist = dist - 1e-3;
                    float atten = 1.0 / max(dist * dist, 1e-4);
                    lightRadiance = light.color.rgb * light.color.a * atten;
                }

                float NdotL = dot(N, Ldir);

                if (NdotL > 0.0) {
                    Ray shadowRay;
                    shadowRay.origin = hitPoint + N * 1e-4;
                    shadowRay.dir    = Ldir;

                    if (!intersectSceneShadow(shadowRay, maxDist)) {
                        radiance += throughput * albedo * (1.0 - mat.metallic) * lightRadiance * NdotL;
                    }
                }
            }

            ray.dir    = cosineHemisphere(N, seed);
            ray.origin = hitPoint + N * 1e-4;
            throughput *= albedo * (1.0 - mat.metallic);
        }

        // Russian roulette
        float p = max(throughput.r, max(throughput.g, throughput.b));
        if (bounce > 3 && rand(seed) > p) break;
        throughput /= max(p, 1e-4);
    }

    return radiance;
}

// ---- Ray tracing classique (Whitted) ----
// Éclairage direct déterministe (ombres dures, un seul rayon par lumière —
// pas de sampleConeDirection/samplePointOnSphere, donc pas de bruit) ; les
// surfaces spéculaires/transparentes continuent en réflexion/réfraction,
// les surfaces diffuses s'arrêtent après leur éclairage direct (pas de GI
// stochastique, remplacée par un ambiant plat pour éviter le noir pur).
vec3 traceRayTraced(Ray ray) {
    vec3 radiance   = vec3(0.0);
    vec3 throughput = vec3(1.0);

    for (int bounce = 0; bounce < maxBounces; ++bounce) {
        HitInfo hit = intersectScene(ray);

        if (!hit.hit) {
            float t   = 0.5 * (ray.dir.y + 1.0);
            vec3  sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
            radiance += throughput * sky;
            break;
        }

        Material mat      = materials[hit.materialIndex];
        vec3     albedo   = mat.albedo.rgb;
        vec3     N        = hit.normal;
        vec3     hitPoint = ray.origin + ray.dir * hit.t;

        if (mat.emissive > 0.0) {
            radiance += throughput * albedo * mat.emissive;
            break;
        }

        vec3 direct = vec3(0.0);
        for (int li = 0; li < lightCount; ++li) {
            Light light = lights[li];

            vec3  Ldir;
            float maxDist;
            vec3  lightRadiance;

            if (light.type == 0) {
                Ldir          = light.positionOrDirection.xyz; // pas de jitter : ombre dure
                maxDist       = 1e30;
                lightRadiance = light.color.rgb * light.color.a;
            } else {
                vec3  toLight = light.positionOrDirection.xyz - hitPoint;
                float dist    = length(toLight);
                Ldir          = toLight / max(dist, 1e-6);
                maxDist       = dist - 1e-3;
                lightRadiance = light.color.rgb * light.color.a / max(dist * dist, 1e-4);
            }

            float NdotL = dot(N, Ldir);
            if (NdotL > 0.0) {
                Ray shadowRay;
                shadowRay.origin = hitPoint + N * 1e-4;
                shadowRay.dir    = Ldir;

                if (!intersectSceneShadow(shadowRay, maxDist))
                    direct += albedo * (1.0 - mat.metallic) * lightRadiance * NdotL;
            }
        }

        // Ambiant plat minimal : pas de GI diffuse dans ce mode, donc rien
        // n'éclairerait les zones hors de portée directe des lumières.
        vec3 ambient = albedo * 0.05;
        radiance += throughput * (direct + ambient);

        // Continue uniquement en réflexion/réfraction (déterministe, pas de
        // tirage aléatoire) : c'est la partie "récursive" du ray tracing
        // classique. Une surface purement diffuse s'arrête ici.
        float cosTheta   = max(dot(-ray.dir, N), 0.0);
        float f0         = mix(0.04, 1.0, mat.metallic);
        float F          = fresnel(cosTheta, f0);
        float specWeight = max(F, mat.metallic);
        bool  isTransparent = mat.albedo.a < 1.0;

        if (isTransparent) {
            bool  tir;
            float eta        = dot(ray.dir, N) < 0.0 ? (1.0 / mat.ior) : mat.ior;
            vec3  refractDir = refract(ray.dir, N, eta, tir);
            ray.dir    = tir ? reflect(ray.dir, N) : refractDir;
            ray.origin = hitPoint - N * 1e-4;
            throughput *= albedo * (1.0 - specWeight);
        } else if (specWeight > 0.02) {
            ray.dir    = reflect(ray.dir, N);
            ray.origin = hitPoint + N * 1e-4;
            throughput *= mix(vec3(1.0), albedo, mat.metallic) * specWeight;
        } else {
            break;
        }
    }

    return radiance;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= width || coord.y >= height) return;

    // TAA doit varier son jitter à CHAQUE frame (même en Realtime, où
    // `frameIndex` reste figé à 0) : il a son propre compteur dédié.
    // None/FXAA n'ont pas besoin de variation de seed pour le jitter
    // puisqu'ils n'en appliquent pas, mais le path tracer s'en sert quand
    // même pour ses propres tirages aléatoires (rebonds, NEE).
    int seedFrame = (aaMode == 2) ? taaFrameIndex : frameIndex;
    uint seed = uint(coord.x + coord.y * width) ^ uint(seedFrame * 2654435761u);

    // None (0) et FXAA (3) : échantillon net au centre du pixel — le
    // supersampling (SSAA) ou le lissage temporel (TAA) fourniraient un
    // jitter, FXAA préfère une image stable à filtrer en post-process.
    bool jitterEnabled = (aaMode == 1 || aaMode == 2);

    vec3 color = vec3(0.0);
    for (int s = 0; s < samplesPerPixel; ++s) {
        vec2 jitter = jitterEnabled ? (vec2(rand(seed), rand(seed)) - 0.5) : vec2(0.0);
        vec2 uv     = (vec2(coord) + jitter) / vec2(width, height) * 2.0 - 1.0;
        uv.x *= camAspect;

        float scale = tan(camFov * 0.5);
        vec3  dir   = normalize(
            camForward.xyz +
            uv.x * scale * camRight.xyz +
            uv.y * scale * camUp.xyz
        );

        Ray ray;
        ray.origin = camPosition.xyz;
        ray.dir    = dir;
        color += (technique == 1) ? traceRayTraced(ray) : tracePath(ray, seed);
    }
    color /= float(samplesPerPixel);

    vec3 resolved;
    if (aaMode == 2) {
        // TAA : moyenne mobile exponentielle avec l'historique, indépendante
        // du compteur d'accumulation du path tracer — c'est ce qui la fait
        // fonctionner aussi en RenderMode::Realtime.
        vec3 prevColor = imageLoad(u_Accum, coord).rgb;
        resolved = (taaFrameIndex == 0) ? color : mix(color, prevColor, taaBlendFactor);
        imageStore(u_Accum, coord, vec4(resolved, 1.0));
    } else {
        // None / SSAA / FXAA : moyenne progressive classique (fonctionne
        // comme avant l'ajout des modes AA).
        vec4 prev  = imageLoad(u_Accum, coord);
        vec4 accum = (frameIndex == 0)
            ? vec4(color, 1.0)
            : vec4(prev.rgb + color, prev.a + 1.0);
        imageStore(u_Accum, coord, accum);
        resolved = accum.rgb / accum.a;
    }

    // Tone mapping ACES + exposition
    vec3 result = resolved * exposure;
    result = (result * (2.51 * result + 0.03)) / (result * (2.43 * result + 0.59) + 0.14);
    result = pow(clamp(result, 0.0, 1.0), vec3(1.0 / 2.2));

    imageStore(u_Output, coord, vec4(result, 1.0));
}
)";

	// ----------------------------------------------------------------
	// Blit vertex + fragment (quad plein �cran)
	// ----------------------------------------------------------------

	static const char* s_BlitVertSrc = R"(
#version 430 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)";

	static const char* s_BlitFragSrc = R"(
#version 430 core
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Texture;
void main() {
    fragColor = texture(u_Texture, v_UV);
}
)";

	// ----------------------------------------------------------------
	// FXAA � post-process par d�tection de contraste de luminance.
	// Version simplifi�e "maison" (pas le FXAA 3.11 de NVIDIA) : d�tecte les
	// bords via le contraste de luminance entre voisins directs, puis floute
	// le long de la direction dominante du gradient (approxim�e via les 4
	// voisins diagonaux) proportionnellement � ce contraste. Moins pr�cis que
	// l'algorithme complet mais bien moins de code, et suffisant pour lisser
	// les cr�nelures d'un rendu sans jitter.
	// ----------------------------------------------------------------

	static const char* s_FXAAFragSrc = R"(
#version 430 core
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Texture;
uniform vec2  u_TexelSize;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec3 rgbCenter = texture(u_Texture, v_UV).rgb;

    vec2 t = u_TexelSize;
    vec3 rgbN  = texture(u_Texture, v_UV + vec2( 0.0, -t.y)).rgb;
    vec3 rgbS  = texture(u_Texture, v_UV + vec2( 0.0,  t.y)).rgb;
    vec3 rgbE  = texture(u_Texture, v_UV + vec2( t.x,  0.0)).rgb;
    vec3 rgbW  = texture(u_Texture, v_UV + vec2(-t.x,  0.0)).rgb;
    vec3 rgbNW = texture(u_Texture, v_UV + vec2(-t.x, -t.y)).rgb;
    vec3 rgbNE = texture(u_Texture, v_UV + vec2( t.x, -t.y)).rgb;
    vec3 rgbSW = texture(u_Texture, v_UV + vec2(-t.x,  t.y)).rgb;
    vec3 rgbSE = texture(u_Texture, v_UV + vec2( t.x,  t.y)).rgb;

    float lC = luma(rgbCenter);
    float lN = luma(rgbN),  lS = luma(rgbS),  lE = luma(rgbE),  lW = luma(rgbW);
    float lNW = luma(rgbNW), lNE = luma(rgbNE), lSW = luma(rgbSW), lSE = luma(rgbSE);

    float lMin = min(lC, min(min(lN, lS), min(lE, lW)));
    float lMax = max(lC, max(max(lN, lS), max(lE, lW)));
    float contrast = lMax - lMin;

    // Zone plate : pas de bord, on sort la couleur brute pour ne pas flouter
    // des d�tails fins pour rien.
    float threshold = max(0.0312, lMax * 0.125);
    if (contrast < threshold) {
        fragColor = vec4(rgbCenter, 1.0);
        return;
    }

    // Sobel simplifi� : direction dominante du gradient (horizontal vs vertical).
    float edgeHoriz = abs(lNW + lN + lNE - lSW - lS - lSE);
    float edgeVert  = abs(lNW + lW + lSW - lNE - lE - lSE);
    bool  horizontal = edgeHoriz >= edgeVert;

    vec3 blurA = horizontal ? rgbN : rgbW;
    vec3 blurB = horizontal ? rgbS : rgbE;
    vec3 blended = (rgbCenter + blurA + blurB) / 3.0;

    // Force le m�lange proportionnellement au contraste local : bord net =
    // lissage fort, bord faible = presque inchang�.
    float blendFactor = clamp(contrast * 4.0, 0.0, 1.0);
    fragColor = vec4(mix(rgbCenter, blended, blendFactor), 1.0);
}
)";

	// ----------------------------------------------------------------
	// Init / Shutdown
	// ----------------------------------------------------------------

	void Renderer::init(int width, int height)
	{
		m_Width = width;
		m_Height = height;

		createTextures();
		createShaders();
		createSSBOs();
		createQuad();
		createFXAAResources();

		FUFU_INFO("Renderer initialized ({}x{})", width, height);
	}

	void Renderer::shutdown()
	{
		glDeleteTextures(1, &m_OutputTexture);
		glDeleteTextures(1, &m_AccumTexture);
		glDeleteTextures(1, &m_FXAATexture);
		glDeleteFramebuffers(1, &m_FXAAFBO);
		glDeleteProgram(m_ComputeProgram);
		glDeleteProgram(m_BlitProgram);
		glDeleteProgram(m_FXAAProgram);
		glDeleteBuffers(1, &m_TriangleSSBO);
		glDeleteBuffers(1, &m_MaterialSSBO);
		glDeleteBuffers(1, &m_BLASNodeSSBO);
		glDeleteBuffers(1, &m_InstanceSSBO);
		glDeleteBuffers(1, &m_TLASNodeSSBO);
		glDeleteBuffers(1, &m_LightSSBO);
		glDeleteBuffers(1, &m_CameraUBO);
		glDeleteBuffers(1, &m_FrameDataUBO);
		glDeleteVertexArrays(1, &m_QuadVAO);
		glDeleteBuffers(1, &m_QuadVBO);
	}

	void Renderer::createTextures()
	{
		auto makeTexture = [&](uint32_t& id)
		{
			glGenTextures(1, &id);
			glBindTexture(GL_TEXTURE_2D, id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
				m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		};

		makeTexture(m_OutputTexture);
		makeTexture(m_AccumTexture);
	}

	void Renderer::createShaders()
	{
		// Compute
		uint32_t cs = compileShader(GL_COMPUTE_SHADER, s_ComputeSrc);
		m_ComputeProgram = linkProgram({ cs });
		glDeleteShader(cs);

		// Blit
		uint32_t vs = compileShader(GL_VERTEX_SHADER, s_BlitVertSrc);
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, s_BlitFragSrc);
		m_BlitProgram = linkProgram({ vs, fs });
		glDeleteShader(vs);
		glDeleteShader(fs);

		// FXAA (r�utilise le vertex shader du blit, m�me quad plein �cran)
		uint32_t vsFxaa = compileShader(GL_VERTEX_SHADER, s_BlitVertSrc);
		uint32_t fsFxaa = compileShader(GL_FRAGMENT_SHADER, s_FXAAFragSrc);
		m_FXAAProgram = linkProgram({ vsFxaa, fsFxaa });
		glDeleteShader(vsFxaa);
		glDeleteShader(fsFxaa);
	}

	void Renderer::createFXAAResources()
	{
		glGenTextures(1, &m_FXAATexture);
		glBindTexture(GL_TEXTURE_2D, m_FXAATexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
			m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glGenFramebuffers(1, &m_FXAAFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FXAAFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_FXAATexture, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			FUFU_ERROR("Renderer: FXAA framebuffer incomplete");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Renderer::createSSBOs()
	{
		glGenBuffers(1, &m_TriangleSSBO);
		glGenBuffers(1, &m_MaterialSSBO);
		glGenBuffers(1, &m_BLASNodeSSBO);
		glGenBuffers(1, &m_InstanceSSBO);
		glGenBuffers(1, &m_TLASNodeSSBO);
		glGenBuffers(1, &m_LightSSBO);
		glGenBuffers(1, &m_CameraUBO);
		glGenBuffers(1, &m_FrameDataUBO);
	}

	void Renderer::createQuad()
	{
		float quad[] = {
			// pos       uv
			-1.f, -1.f,  0.f, 0.f,
			 1.f, -1.f,  1.f, 0.f,
			 1.f,  1.f,  1.f, 1.f,
			-1.f, -1.f,  0.f, 0.f,
			 1.f,  1.f,  1.f, 1.f,
			-1.f,  1.f,  0.f, 1.f,
		};

		glGenVertexArrays(1, &m_QuadVAO);
		glGenBuffers(1, &m_QuadVBO);
		glBindVertexArray(m_QuadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);
	}

	// ----------------------------------------------------------------
	// Upload sc�ne
	// ----------------------------------------------------------------

	void Renderer::uploadSceneData(Scene& scene)
	{
		m_Triangles.clear();
		m_Materials.clear();
		m_BLASNodes.clear();
		m_Instances.clear();

		auto& am = Fufu::Application::get().getProjectManager().getCurrentProject().getAssetManager();

		// Caméra active : sert à choisir le LOD par distance (voir selectLOD).
		glm::vec3 cameraPos(0.f);
		if (Entity cam = scene.getPrimaryCamera())
			cameraPos = cam.getComponent<TransformComponent>().position;

		// Seuils de distance volontairement simples (pas de hystérésis ni de
		// notion de taille à l'écran) : LOD0 en dessous de 15 unités, LOD1
		// jusqu'à 40, LOD2 au-delà. Un "pop" est possible en traversant un
		// seuil — acceptable pour cette première itération.
		auto selectLOD = [](float distance) -> int
		{
			if (distance > 40.f) return 2;
			if (distance > 15.f) return 1;
			return 0;
		};

		// Cache BLAS par (chemin de mesh, niveau de LOD) : construit une seule
		// fois par combinaison unique, même si N entités la référencent —
		// c'est l'instancing. Reconstruit à chaque frame comme le reste de
		// l'upload (cohérent avec le sculpt/extrude qui mutent LOD0 en
		// mémoire ; voir MeshAsset::invalidateLODs pour les LOD générés).
		struct BLASRef { int nodeOffset; int triOffset; };
		std::unordered_map<std::string, BLASRef> blasCache;

		auto getOrBuildBLAS = [&](const std::string& path, int lod) -> const BLASRef*
		{
			std::string key = path + "#lod" + std::to_string(lod);
			auto found = blasCache.find(key);
			if (found != blasCache.end())
				return &found->second;

			auto meshAsset = am.getMesh(path);
			if (!meshAsset || meshAsset->getSubMeshCount() == 0)
				return nullptr;

			std::vector<GPUTriangle> localTris;
			for (const auto& sub : meshAsset->getLODSubMeshes(lod))
			{
				for (std::size_t i = 0; i + 2 < sub.indices.size(); i += 3)
				{
					const Vertex& a = sub.vertices[sub.indices[i]];
					const Vertex& b = sub.vertices[sub.indices[i + 1]];
					const Vertex& c = sub.vertices[sub.indices[i + 2]];

					GPUTriangle tri{};
					tri.v0 = glm::vec4(a.position, 0.f);
					tri.v1 = glm::vec4(b.position, 0.f);
					tri.v2 = glm::vec4(c.position, 0.f);
					tri.n0 = glm::vec4(a.normal, 0.f);
					tri.n1 = glm::vec4(b.normal, 0.f);
					tri.n2 = glm::vec4(c.normal, 0.f);
					tri.uv0 = a.uv;
					tri.uv1 = b.uv;
					tri.uv2 = c.uv;
					tri.materialIndex = 0; // le matériau vient de l'instance, pas du BLAS partagé
					localTris.push_back(tri);
				}
			}

			if (localTris.empty())
				return nullptr;

			std::vector<GPUBVHNode> localNodes = BVHBuilder::build(localTris);

			BLASRef ref{ static_cast<int>(m_BLASNodes.size()), static_cast<int>(m_Triangles.size()) };
			m_BLASNodes.insert(m_BLASNodes.end(), localNodes.begin(), localNodes.end());
			m_Triangles.insert(m_Triangles.end(), localTris.begin(), localTris.end());

			return &blasCache.emplace(std::move(key), ref).first->second;
		};

		// Meshes "classiques" : le même BLAS (mesh, LOD) peut être partagé par plusieurs instances.
		scene.each<TransformComponent, MeshComponent, MaterialComponent>(
			[&](entt::entity /*e*/,
				TransformComponent& transform,
				MeshComponent& meshComp,
				MaterialComponent& matComp)
		{
			float distance = glm::length(transform.position - cameraPos);
			const BLASRef* blas = getOrBuildBLAS(meshComp.meshPath, selectLOD(distance));
			if (!blas) return;

			int matIdx = static_cast<int>(m_Materials.size());
			GPUMaterial gpuMat;
			gpuMat.albedo = matComp.albedo;
			gpuMat.metallic = matComp.metallic;
			gpuMat.roughness = matComp.roughness;
			gpuMat.emissive = matComp.emissive;
			gpuMat.ior = 1.5f;
			gpuMat.albedoTexIdx = -1;
			m_Materials.push_back(gpuMat);

			GPUInstance inst{};
			inst.transform = transform.getTransform();
			inst.invTransform = glm::inverse(inst.transform);
			inst.materialIndex = matIdx;
			inst.blasNodeOffset = blas->nodeOffset;
			inst.blasTriOffset = blas->triOffset;
			m_Instances.push_back(inst);
		}
		);

		// Groom : géométrie procédurale propre à CHAQUE entité (seed, courbure...),
		// donc pas de partage de BLAS entre groom — mais même chemin d'instanciation
		// que les meshes classiques (une instance, un BLAS dédié).
		scene.each<TransformComponent, MeshComponent, GroomComponent>(
			[&](entt::entity /*e*/,
				TransformComponent& transform,
				MeshComponent& meshComp,
				GroomComponent& groom)
		{
			auto meshAsset = am.getMesh(meshComp.meshPath);
			if (!meshAsset) return;

			std::vector<GPUTriangle> groomTris;
			GroomGenerator::generate(*meshAsset, groom, groomTris);
			if (groomTris.empty()) return;

			std::vector<GPUBVHNode> groomNodes = BVHBuilder::build(groomTris);

			int nodeOffset = static_cast<int>(m_BLASNodes.size());
			int triOffset = static_cast<int>(m_Triangles.size());
			m_BLASNodes.insert(m_BLASNodes.end(), groomNodes.begin(), groomNodes.end());
			m_Triangles.insert(m_Triangles.end(), groomTris.begin(), groomTris.end());

			int matIdx = static_cast<int>(m_Materials.size());
			GPUMaterial gpuMat;
			gpuMat.albedo = groom.color;
			gpuMat.metallic = 0.f;
			gpuMat.roughness = 0.9f;
			gpuMat.emissive = 0.f;
			gpuMat.ior = 1.4f;
			gpuMat.albedoTexIdx = -1;
			m_Materials.push_back(gpuMat);

			GPUInstance inst{};
			inst.transform = transform.getTransform();
			inst.invTransform = glm::inverse(inst.transform);
			inst.materialIndex = matIdx;
			inst.blasNodeOffset = nodeOffset;
			inst.blasTriOffset = triOffset;
			m_Instances.push_back(inst);
		}
		);

		// Lumières : la Directional dérive sa direction de la ROTATION de
		// l'entité (même convention que la caméra), la Point de sa POSITION —
		// pas de champ dédié, les gizmos de transform existants suffisent.
		m_Lights.clear();
		scene.each<TransformComponent, LightComponent>(
			[&](entt::entity /*e*/, TransformComponent& transform, LightComponent& light)
		{
			GPULight gpuLight{};
			gpuLight.color = glm::vec4(light.color, light.intensity);
			gpuLight.radius = light.radius;

			if (light.type == LightType::Directional)
			{
				glm::quat rotation = glm::quat(transform.rotation);
				glm::vec3 forward = glm::normalize(rotation * glm::vec3(0.f, 0.f, -1.f));
				gpuLight.positionOrDirection = glm::vec4(-forward, 0.f); // de la surface VERS la lumière
				gpuLight.type = 0;
			}
			else
			{
				gpuLight.positionOrDirection = glm::vec4(transform.position, 0.f);
				gpuLight.type = 1;
			}

			m_Lights.push_back(gpuLight);
		}
		);

		// TLAS : une boîte englobante MONDE par instance, dérivée des 8 coins
		// de la boîte locale de la racine de son BLAS.
		std::vector<glm::vec3> instBoundsMin(m_Instances.size());
		std::vector<glm::vec3> instBoundsMax(m_Instances.size());

		for (std::size_t i = 0; i < m_Instances.size(); ++i)
		{
			const GPUInstance& inst = m_Instances[i];
			const GPUBVHNode& root = m_BLASNodes[static_cast<std::size_t>(inst.blasNodeOffset)];

			glm::vec3 lmin = glm::vec3(root.boundsMin);
			glm::vec3 lmax = glm::vec3(root.boundsMax);

			glm::vec3 worldMin(1e30f), worldMax(-1e30f);
			for (int c = 0; c < 8; ++c)
			{
				glm::vec3 corner(
					(c & 1) ? lmax.x : lmin.x,
					(c & 2) ? lmax.y : lmin.y,
					(c & 4) ? lmax.z : lmin.z);
				glm::vec3 worldCorner = glm::vec3(inst.transform * glm::vec4(corner, 1.f));
				worldMin = glm::min(worldMin, worldCorner);
				worldMax = glm::max(worldMax, worldCorner);
			}

			instBoundsMin[i] = worldMin;
			instBoundsMax[i] = worldMax;
		}

		std::vector<int> order;
		m_TLASNodes = BVHBuilder::buildFromBounds(instBoundsMin, instBoundsMax, order);

		std::vector<GPUInstance> reorderedInstances(m_Instances.size());
		for (std::size_t i = 0; i < order.size(); ++i)
			reorderedInstances[i] = m_Instances[static_cast<std::size_t>(order[i])];
		m_Instances = std::move(reorderedInstances);

		// Upload triangles (BLAS, espace local, concaténés par mesh unique)
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TriangleSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Triangles.size() * sizeof(GPUTriangle),
			m_Triangles.data(), GL_DYNAMIC_DRAW);

		// Upload materials
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Materials.size() * sizeof(GPUMaterial),
			m_Materials.data(), GL_DYNAMIC_DRAW);

		// Upload nœuds BLAS
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BLASNodeSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_BLASNodes.size() * sizeof(GPUBVHNode),
			m_BLASNodes.data(), GL_DYNAMIC_DRAW);

		// Upload instances
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_InstanceSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Instances.size() * sizeof(GPUInstance),
			m_Instances.data(), GL_DYNAMIC_DRAW);

		// Upload TLAS
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TLASNodeSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_TLASNodes.size() * sizeof(GPUBVHNode),
			m_TLASNodes.data(), GL_DYNAMIC_DRAW);

		// Upload lumières
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_LightSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Lights.size() * sizeof(GPULight),
			m_Lights.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		FUFU_TRACE("Scene uploaded: {} instances, {} unique BLAS triangles, {} materials",
			m_Instances.size(), m_Triangles.size(), m_Materials.size());
	}

	// ----------------------------------------------------------------
	// Render
	// ----------------------------------------------------------------

	void Renderer::renderScene(Scene& scene)
	{
		// Upload sc�ne si chang�e
		if (sceneNeedsUpdate(scene))
		{
			uploadSceneData(scene);
			if (m_Settings.resetOnMove)
				resetAccumulation();
		}

		// Cam�ra
		Entity cam = scene.getPrimaryCamera();
		if (!cam)
		{
			// Sans cam�ra, rien � rendre : effacer la sortie plut�t que de
			// laisser l'image de la sc�ne pr�c�dente affich�e � l'�cran.
			clearOutput();
			return;
		}

		auto& camTransform = cam.getComponent<TransformComponent>();
		auto& camComponent = cam.getComponent<CameraComponent>();

		glm::mat4 view = glm::inverse(camTransform.getTransform());
		glm::vec3 forward = glm::normalize(-glm::vec3(view[0][2], view[1][2], view[2][2]));
		glm::vec3 right = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
		glm::vec3 up = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));

		GPUCamera gpuCam;
		gpuCam.position = glm::vec4(camTransform.position.x, camTransform.position.y, camTransform.position.z, 1.f);
		gpuCam.forward = glm::vec4(forward, 0.f);
		gpuCam.right = glm::vec4(right, 0.f);
		gpuCam.up = glm::vec4(up, 0.f);
		gpuCam.fov = camComponent.fov;
		gpuCam.aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
		gpuCam.nearPlane = camComponent.nearPlane;

		glBindBuffer(GL_UNIFORM_BUFFER, m_CameraUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUCamera), &gpuCam, GL_DYNAMIC_DRAW);

		// Frame data
		GPUFrameData frameData;
		frameData.frameIndex = (m_Settings.mode == RenderMode::Accumulation)
			? m_FrameIndex : 0;
		frameData.maxBounces = m_Settings.maxBounces;
		frameData.samplesPerPixel = m_Settings.samplesPerPixel;
		frameData.exposure = m_Settings.exposure;
		frameData.width = m_Width;
		frameData.height = m_Height;
		// Réutilisé côté shader comme "nombre d'instances" (garde anti scène
		// vide pour le TLAS) — voir intersectScene dans le compute shader.
		frameData.triangleCount = static_cast<int>(m_Instances.size());
		frameData.materialCount = static_cast<int>(m_Materials.size());
		frameData.lightCount = static_cast<int>(m_Lights.size());
		frameData.technique = static_cast<int>(m_Settings.technique);
		frameData.aaMode = static_cast<int>(m_Settings.aaMode);
		frameData.taaFrameIndex = m_TAAFrameIndex;
		frameData.taaBlendFactor = m_Settings.taaBlendFactor;

		glBindBuffer(GL_UNIFORM_BUFFER, m_FrameDataUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUFrameData), &frameData, GL_DYNAMIC_DRAW);

		computePass();
		//blitPass();

		if (m_Settings.aaMode == AAMode::FXAA)
			fxaaPass();

		// Incr�menter uniquement en accumulation et si pas � la limite
		if (m_Settings.mode == RenderMode::Accumulation &&
			m_FrameIndex < m_Settings.maxAccumFrames)
			++m_FrameIndex;

		// Contrairement � m_FrameIndex, le compteur TAA avance toujours : c'est
		// ce qui lui permet de lisser aussi en RenderMode::Realtime.
		++m_TAAFrameIndex;
	}

	void Renderer::computePass()
	{
		glUseProgram(m_ComputeProgram);

		// Textures image
		glBindImageTexture(0, m_OutputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindImageTexture(1, m_AccumTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

		// Buffers
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_TriangleSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_MaterialSSBO);
		glBindBufferBase(GL_UNIFORM_BUFFER, 4, m_CameraUBO);
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, m_FrameDataUBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_BLASNodeSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_InstanceSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, m_TLASNodeSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, m_LightSSBO);

		// Dispatch � groupes de 16x16
		int gx = (m_Width + 15) / 16;
		int gy = (m_Height + 15) / 16;
		glDispatchCompute(gx, gy, 1);

		// Barri�re avant lecture par le fragment shader
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	void Renderer::blitPass()
	{
		glUseProgram(m_BlitProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_OutputTexture);
		glUniform1i(glGetUniformLocation(m_BlitProgram, "u_Texture"), 0);

		glBindVertexArray(m_QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	void Renderer::fxaaPass()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_FXAAFBO);
		glViewport(0, 0, m_Width, m_Height);

		glUseProgram(m_FXAAProgram);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_OutputTexture);
		glUniform1i(glGetUniformLocation(m_FXAAProgram, "u_Texture"), 0);
		glUniform2f(glGetUniformLocation(m_FXAAProgram, "u_TexelSize"),
			1.f / static_cast<float>(m_Width), 1.f / static_cast<float>(m_Height));

		glBindVertexArray(m_QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// ----------------------------------------------------------------
	// Resize / Reset
	// ----------------------------------------------------------------

	void Renderer::resize(int width, int height)
	{
		m_Width = width;
		m_Height = height;

		glDeleteTextures(1, &m_OutputTexture);
		glDeleteTextures(1, &m_AccumTexture);
		glDeleteTextures(1, &m_FXAATexture);
		glDeleteFramebuffers(1, &m_FXAAFBO);
		createTextures();
		createFXAAResources();
		resetAccumulation();
	}

	void Renderer::resetAccumulation()
	{
		m_FrameIndex = 0;
		m_TAAFrameIndex = 0;
	}

	void Renderer::clearOutput()
	{
		std::size_t pixelCount = static_cast<std::size_t>(m_Width) * static_cast<std::size_t>(m_Height) * 4;
		if (pixelCount == 0) return;

		static std::vector<float> s_ClearBuffer;
		if (s_ClearBuffer.size() != pixelCount)
		{
			s_ClearBuffer.resize(pixelCount);
			for (std::size_t i = 0; i < pixelCount; i += 4)
			{
				s_ClearBuffer[i + 0] = 0.15f;
				s_ClearBuffer[i + 1] = 0.15f;
				s_ClearBuffer[i + 2] = 0.16f;
				s_ClearBuffer[i + 3] = 1.f;
			}
		}

		// Les deux textures potentiellement affichées (voir getOutputTextureID)
		// doivent être effacées, sinon FXAA continuerait d'afficher l'ancienne
		// scène alors que m_OutputTexture, lui, est bien effacé.
		glBindTexture(GL_TEXTURE_2D, m_OutputTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_FLOAT, s_ClearBuffer.data());

		glBindTexture(GL_TEXTURE_2D, m_FXAATexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_FLOAT, s_ClearBuffer.data());
	}

	bool Renderer::sceneNeedsUpdate(Scene& /*scene*/)
	{
		// Pour l'instant on upload � chaque frame.
		// Tu peux ajouter un syst�me de version sur Scene plus tard
		// pour ne re-uploader que si des composants ont chang�.
		return true;
	}

	// ----------------------------------------------------------------
	// Compilation shaders
	// ----------------------------------------------------------------

	uint32_t Renderer::compileShader(uint32_t type, const std::string& source)
	{
		uint32_t shader = glCreateShader(type);
		const char* src = source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		int success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			char log[1024];
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			FUFU_ERROR("Shader compile error:\n{}", log);
		}
		return shader;
	}

	uint32_t Renderer::linkProgram(std::initializer_list<uint32_t> shaders)
	{
		uint32_t program = glCreateProgram();
		for (uint32_t s : shaders)
			glAttachShader(program, s);
		glLinkProgram(program);

		int success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			char log[1024];
			glGetProgramInfoLog(program, sizeof(log), nullptr, log);
			FUFU_ERROR("Program link error:\n{}", log);
		}
		return program;
	}

}