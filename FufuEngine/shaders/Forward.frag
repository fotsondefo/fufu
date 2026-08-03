#version 430 core

// PBR Cook-Torrance (GGX NDF, Smith geometry, Fresnel-Schlick) + IBL ambient
// for forward rendering. HDR linear output — tone mapping applied by
// ToneMappingPass. Reuses GBuffer.vert for vertex fetching.

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_UV;
flat in int v_MaterialIndex;

out vec4 fragColor;

struct Material {
    vec4  albedo;
    float metallic;
    float roughness;
    float emissive;
    float ior;
    int   albedoTexIdx;
    int   normalTexIdx;
    int   ormTexIdx;
    float _pad;
};

struct Light {
    vec4  positionOrDir;
    vec4  color;
    float radius;
    int   type;
    float _pad[2];
};

layout(std430, binding = 3) readonly buffer MaterialBuffer { Material materials[]; };
layout(std430, binding = 9) readonly buffer LightBuffer    { Light lights[]; };

layout(binding =  1) uniform sampler2D u_MaterialTextures[16];
layout(binding = 22) uniform sampler2D u_IrradianceMap;
layout(binding = 23) uniform sampler2D u_PrefilteredEnvMap;
layout(binding = 24) uniform sampler2D u_BrdfLut;

uniform int u_IBLEnabled;

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

const float PI       = 3.14159265359;
const int   IBL_MIPS = 5;

vec2 equirectUV(vec3 dir) {
    return vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5,
                acos(clamp(dir.y, -1.0, 1.0)) / PI);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
              * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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

vec3 pbrLight(vec3 N, vec3 V, vec3 L, vec3 albedo,
              float metallic, float roughness, vec3 F0, vec3 radiance)
{
    vec3  H     = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3  F   = fresnelSchlick(HdotV, F0);
    float D   = ggxNDF(NdotH, roughness);
    float G   = schlickGGX(NdotV, roughness) * schlickGGX(NdotL, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-6);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    Material mat = materials[v_MaterialIndex];

    vec3 albedo = mat.albedo.rgb;
    if (mat.albedoTexIdx >= 0)
        albedo = texture(u_MaterialTextures[mat.albedoTexIdx], v_UV).rgb;

    float metallic  = mat.metallic;
    float roughness = mat.roughness;
    if (mat.ormTexIdx >= 0) {
        vec3 orm = texture(u_MaterialTextures[mat.ormTexIdx], v_UV).rgb;
        roughness = orm.g;
        metallic  = orm.b;
        // AO from ORM.r is baked into ambient below
    }
    roughness = max(roughness, 0.04);

    vec3 N = normalize(v_WorldNormal);
    if (mat.normalTexIdx >= 0) {
        vec3 q1  = dFdx(v_WorldPos);
        vec3 q2  = dFdy(v_WorldPos);
        vec2 st1 = dFdx(v_UV);
        vec2 st2 = dFdy(v_UV);
        float r  = 1.0 / max(abs(st1.x * st2.y - st2.x * st1.y), 1e-6);
        vec3 T   = normalize((q1 * st2.y - q2 * st1.y) * r);
        vec3 B   = normalize((q2 * st1.x - q1 * st2.x) * r);
        vec3 tsN = texture(u_MaterialTextures[mat.normalTexIdx], v_UV).rgb * 2.0 - 1.0;
        N = normalize(mat3(T, B, N) * tsN);
    }

    float aoORM = (mat.ormTexIdx >= 0)
        ? texture(u_MaterialTextures[mat.ormTexIdx], v_UV).r
        : 1.0;

    float f0Dielectric = pow((mat.ior - 1.0) / (mat.ior + 1.0), 2.0);
    vec3  V         = normalize(camPos - v_WorldPos);
    float NdotV     = max(dot(N, V), 1e-4);
    vec3  F0        = mix(vec3(f0Dielectric), albedo, metallic);

    // ── Direct lighting ───────────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);
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

        Lo += pbrLight(N, V, L, albedo, metallic, roughness, F0, radiance);
    }

    // ── Ambient (IBL or flat fallback) ────────────────────────────────────────
    vec3 ambient;
    if (u_IBLEnabled != 0) {
        vec3 kS = fresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        vec3 irradiance = texture(u_IrradianceMap, equirectUV(N)).rgb;
        vec3 diffuse    = kD * albedo * irradiance;

        vec3  R           = reflect(-V, N);
        float lod         = roughness * float(IBL_MIPS - 1);
        vec3  prefiltered = textureLod(u_PrefilteredEnvMap, equirectUV(R), lod).rgb;
        vec2  brdf        = texture(u_BrdfLut, vec2(NdotV, roughness)).rg;
        vec3  specular    = prefiltered * (F0 * brdf.x + brdf.y);

        ambient = (diffuse + specular) * aoORM;
    } else {
        ambient = 0.03 * albedo * aoORM;
    }

    vec3 color = Lo + ambient;
    color += albedo * mat.emissive;

    // HDR linear output — ToneMappingPass applies the operator and gamma.
    fragColor = vec4(color * exposure, 1.0);
}
