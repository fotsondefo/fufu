#version 430 core

// PBR Cook-Torrance (GGX NDF, Smith geometry, Fresnel-Schlick) pour le
// forward rendering. Réutilise GBuffer.vert pour le fetch des sommets.

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_UV;
flat in int v_MaterialIndex;

out vec4 fragColor;

struct Material {
    vec4  albedo;       // rgb + alpha
    float metallic;
    float roughness;
    float emissive;
    float ior;
    int   albedoTexIdx; // -1 = pas de texture
    float _pad0, _pad1, _pad2;
};

struct Light {
    vec4  positionOrDir; // xyz = dir-vers-lumière (Directional) ou position (Point)
    vec4  color;         // rgb + intensité
    float radius;
    int   type;          // 0=Directional, 1=Point
    float _pad[2];
};

layout(std430, binding = 3) readonly buffer MaterialBuffer { Material materials[]; };
layout(std430, binding = 9) readonly buffer LightBuffer    { Light lights[]; };

layout(binding = 1) uniform sampler2D u_MaterialTextures[16];

layout(std140, binding = 0) uniform FrameBlock {
    mat4  viewProj;
    vec3  camPos;      float _p0;
    vec3  camForward;  float camFov;
    vec3  camRight;    float camAspect;
    vec3  camUp;       float exposure;
    int   lightCount;
    int   hasSkybox;
    float skyboxIntensity;
    float _p1;
};

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ggxNDF(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-6);
}

float schlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}

vec3 pbrLight(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, vec3 radiance) {
    vec3  H     = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3  F0  = mix(vec3(0.04), albedo, metallic);
    vec3  F   = fresnelSchlick(HdotV, F0);
    float D   = ggxNDF(NdotH, roughness);
    float G   = schlickGGX(NdotV, roughness) * schlickGGX(NdotL, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-6);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    Material mat = materials[v_MaterialIndex];

    vec3 albedo = mat.albedo.rgb;
    if (mat.albedoTexIdx >= 0)
        albedo = texture(u_MaterialTextures[mat.albedoTexIdx], v_UV).rgb;

    vec3 N = normalize(v_WorldNormal);
    vec3 V = normalize(camPos - v_WorldPos);

    vec3 color = vec3(0.0);
    for (int i = 0; i < lightCount; ++i) {
        Light light = lights[i];
        vec3 L;
        vec3 radiance;

        if (light.type == 0) {
            L        = normalize(light.positionOrDir.xyz);
            radiance = light.color.rgb * light.color.a;
        } else {
            vec3  toLight     = light.positionOrDir.xyz - v_WorldPos;
            float dist        = length(toLight);
            L                 = toLight / dist;
            float attenuation = 1.0 / max(dist * dist, 0.01);
            radiance          = light.color.rgb * light.color.a * attenuation;
        }

        color += pbrLight(N, V, L, albedo, mat.metallic, mat.roughness, radiance);
    }

    color += 0.03 * albedo;               // ambiant plat
    color += albedo * mat.emissive;       // émissif

    color = aces(color * exposure);
    color = pow(color, vec3(1.0 / 2.2));

    fragColor = vec4(color, 1.0);
}
