#include "depch.h"
#include "Renderer/Passes/ComputePass.h"
#include "Renderer/ShaderUtils.h"

namespace Fufu
{

	// ----------------------------------------------------------------
	// Compute shader — path tracing complet
	// GI + shadows + reflections + réfraction
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

// Binding 0 ici partage le même numéro que l'image u_Output (binding = 0 sur
// `image2D`) : ce n'est PAS un conflit, les unités d'image et les unités de
// texture (sampler) sont deux espaces de binding distincts en GLSL/OpenGL.
layout(binding = 0) uniform sampler2D u_Skybox;

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
    int   hasSkybox;       // 1 = échantillonner u_Skybox, 0 = dégradé de ciel procédural
    float skyboxIntensity;
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

// ---- Géométrie ----
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

// Couleur vue par un rayon qui ne touche aucune géométrie : soit une texture
// HDRI équirectangulaire (mapping longitude/latitude standard), soit le
// dégradé de ciel procédural existant si aucun skybox n'est configuré.
vec3 sampleSky(vec3 dir) {
    if (hasSkybox == 1) {
        float u = atan(dir.z, dir.x) * (1.0 / 6.28318530718) + 0.5;
        float v = acos(clamp(dir.y, -1.0, 1.0)) * (1.0 / 3.14159265359);
        return texture(u_Skybox, vec2(u, v)).rgb * skyboxIntensity;
    }

    float t = 0.5 * (dir.y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

// ---- Path tracing ----
vec3 tracePath(Ray ray, inout uint seed) {
    vec3 radiance    = vec3(0.0);
    vec3 throughput  = vec3(1.0);

    for (int bounce = 0; bounce < maxBounces; ++bounce) {
        HitInfo hit = intersectScene(ray);

        if (!hit.hit) {
            radiance += throughput * sampleSky(ray.dir);
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

        // Émission
        if (mat.emissive > 0.0) {
            radiance += throughput * albedo * mat.emissive;
            break;
        }

        float cosTheta = max(dot(-ray.dir, N), 0.0);
        float f0       = mix(0.04, 1.0, mat.metallic);
        float F        = fresnel(cosTheta, f0);

        // Choix stochastique : réflexion spéculaire / diffuse / réfraction
        float specProb = mix(F, 1.0, mat.metallic);
        float refrProb = (mat.albedo.a < 1.0) ? (1.0 - specProb) * (1.0 - mat.albedo.a) : 0.0;
        float r        = rand(seed);

        if (r < specProb) {
            // Réflexion spéculaire (GGX simplifié via roughness)
            vec3 roughDir = cosineHemisphere(N, seed);
            vec3 specDir  = reflect(ray.dir, N);
            ray.dir    = normalize(mix(specDir, roughDir, mat.roughness * mat.roughness));
            ray.origin = hitPoint + N * 1e-4;
            throughput *= albedo;
        }
        else if (r < specProb + refrProb) {
            // Réfraction (verre)
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
            radiance += throughput * sampleSky(ray.dir);
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

	void ComputePass::init()
	{
		uint32_t cs = compileShader(GL_COMPUTE_SHADER, s_ComputeSrc);
		m_Program = linkProgram({ cs });
		glDeleteShader(cs);

		glGenBuffers(1, &m_CameraUBO);
		glGenBuffers(1, &m_FrameDataUBO);
	}

	void ComputePass::shutdown()
	{
		glDeleteProgram(m_Program);
		glDeleteBuffers(1, &m_CameraUBO);
		glDeleteBuffers(1, &m_FrameDataUBO);
	}

	void ComputePass::execute(const GPUScene& scene, const GPUCamera& camera, const GPUFrameData& frameData,
		uint32_t outputTexture, uint32_t accumTexture, uint32_t skyboxTexture, int width, int height)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_CameraUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUCamera), &camera, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_UNIFORM_BUFFER, m_FrameDataUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUFrameData), &frameData, GL_DYNAMIC_DRAW);

		glUseProgram(m_Program);

		glBindImageTexture(0, outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindImageTexture(1, accumTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, skyboxTexture);

		scene.bind();

		glBindBufferBase(GL_UNIFORM_BUFFER, 4, m_CameraUBO);
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, m_FrameDataUBO);

		// Dispatch à groupes de 16x16
		int gx = (width + 15) / 16;
		int gy = (height + 15) / 16;
		glDispatchCompute(gx, gy, 1);

		// Barrière avant lecture par un pass suivant (FXAA) ou par ImGui.
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	}

}
