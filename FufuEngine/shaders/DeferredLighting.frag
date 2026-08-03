#version 430 core

// Deferred lighting pass: reads the G-Buffer produced by GBufferPass and
// applies PBR Cook-Torrance + IBL ambient. HDR linear output —
// ToneMappingPass applies the operator and gamma. Supports PCF 3x3 shadow
// mapping for directional lights (ShadowBlock binding=2, sampler binding=20).

in vec2 v_UV;
out vec4 fragColor;

struct Material {
    vec4  albedo;
    float metallic;
    float roughness;
    float emissive;
    float ior;
    int   albedoTexIdx;
    int   normalTexIdx; // unused in lighting pass (normal already in GBuffer)
    int   ormTexIdx;    // ORM packed: R=AO, G=Roughness, B=Metallic
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

layout(binding = 0)  uniform sampler2D u_Skybox;
layout(binding = 1)  uniform sampler2D u_MaterialTextures[16];
layout(binding = 17) uniform sampler2D u_GBuffer_Position;
layout(binding = 18) uniform sampler2D u_GBuffer_Normal;
layout(binding = 19) uniform sampler2D u_GBuffer_UV;
layout(binding = 20) uniform sampler2DShadow u_ShadowMap;
layout(binding = 21) uniform sampler2D       u_SSAO;

// IBL textures (baked from the environment map)
layout(binding = 22) uniform sampler2D u_IrradianceMap;   // diffuse ambient
layout(binding = 23) uniform sampler2D u_PrefilteredEnvMap; // specular ambient (has mipmaps)
layout(binding = 24) uniform sampler2D u_BrdfLut;         // split-sum table

uniform int u_SSAOEnabled;
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

layout(std140, binding = 2) uniform ShadowBlock {
    mat4  lightSpaceMatrix;
    float shadowBias;
    int   shadowEnabled;
    float _spad[2];
};

const float PI          = 3.14159265359;
const int   IBL_MIPS    = 5; // must match IBLBaker::k_MipLevels

// ── Equirectangular helpers ───────────────────────────────────────────────────

vec2 equirectUV(vec3 dir) {
    return vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5,
                acos(clamp(dir.y, -1.0, 1.0)) / PI);
}

vec3 sampleSky(vec3 dir) {
    if (hasSkybox == 1)
        return texture(u_Skybox, equirectUV(dir)).rgb * skyboxIntensity;
    float t = 0.5 * (dir.y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

// ── BRDF helpers ─────────────────────────────────────────────────────────────

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Roughness-attenuated Fresnel for IBL ambient (prevents over-darkening at
// grazing angles on rough surfaces — without it rough metals turn black).
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

// Cook-Torrance BRDF for a single punctual light.
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

// ── Shadow ────────────────────────────────────────────────────────────────────

float computeShadow(vec3 worldPos) {
    if (shadowEnabled == 0) return 1.0;

    vec4 fragPosLS  = lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    projCoords      = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 1.0;

    float shadow    = 0.0;
    vec2  texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += texture(u_ShadowMap,
                vec3(projCoords.xy + vec2(x, y) * texelSize,
                     projCoords.z - shadowBias));

    return shadow / 9.0;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main() {
    vec4 gPos = texture(u_GBuffer_Position, v_UV);

    // Sky pixel (GBuffer position.w == 0)
    if (gPos.w < 0.5) {
        vec2  ndc   = v_UV * 2.0 - 1.0;
        float scale = tan(camFov * 0.5);
        vec3  dir   = normalize(camForward
                              + ndc.x * camAspect * scale * camRight
                              + ndc.y             * scale * camUp);
        fragColor = vec4(sampleSky(dir) * exposure, 1.0);
        return;
    }

    vec4 gNrm = texture(u_GBuffer_Normal, v_UV);
    vec4 gUVw = texture(u_GBuffer_UV,     v_UV);

    vec3  worldPos = gPos.xyz;
    vec3  N        = normalize(gNrm.xyz);
    int   matIdx   = int(gNrm.w + 0.5);
    vec2  uv       = gUVw.xy;

    Material mat = materials[matIdx];

    vec3 albedo = mat.albedo.rgb;
    if (mat.albedoTexIdx >= 0)
        albedo = texture(u_MaterialTextures[mat.albedoTexIdx], uv).rgb;

    float metallic  = mat.metallic;
    float roughness = mat.roughness;
    float aoORM     = 1.0;
    if (mat.ormTexIdx >= 0) {
        vec3 orm = texture(u_MaterialTextures[mat.ormTexIdx], uv).rgb;
        aoORM     = orm.r;
        roughness = orm.g;
        metallic  = orm.b;
    }
    roughness = max(roughness, 0.04); // avoid NDF singularity

    float ssao   = (u_SSAOEnabled != 0) ? texture(u_SSAO, v_UV).r : 1.0;
    float ao     = min(ssao, aoORM);

    // IOR-based F0: for dielectrics F0 = ((ior-1)/(ior+1))^2
    float f0Dielectric = pow((mat.ior - 1.0) / (mat.ior + 1.0), 2.0);
    vec3  V         = normalize(camPos - worldPos);
    float NdotV     = max(dot(N, V), 1e-4);
    vec3  F0        = mix(vec3(f0Dielectric), albedo, metallic);
    float shadow = computeShadow(worldPos);

    // ── Direct lighting (punctual lights) ────────────────────────────────────
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lightCount; ++i) {
        Light light = lights[i];
        vec3 L;
        vec3 radiance;

        if (light.type == 0) {
            // Directional light — apply shadow factor
            L        = normalize(light.positionOrDir.xyz);
            radiance = light.color.rgb * light.color.a * shadow;
        } else {
            vec3  toLight     = light.positionOrDir.xyz - worldPos;
            float dist        = length(toLight);
            L                 = toLight / dist;
            float attenuation = 1.0 / max(dist * dist, 0.01);
            radiance          = light.color.rgb * light.color.a * attenuation;
        }

        Lo += pbrLight(N, V, L, albedo, metallic, roughness, F0, radiance);
    }

    // ── Ambient (IBL or flat fallback) ───────────────────────────────────────
    vec3 ambient;
    if (u_IBLEnabled != 0) {
        // Split-sum ambient: diffuse (irradiance) + specular (prefiltered + BRDF LUT).
        // Using fresnelSchlickRoughness so rough metals don't appear pitch-black
        // in areas without direct lights.
        vec3 kS = fresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        // Diffuse: sample the pre-convolved irradiance map at N
        vec3 irradiance = texture(u_IrradianceMap, equirectUV(N)).rgb;
        vec3 diffuse    = kD * albedo * irradiance;

        // Specular: sample the pre-filtered map at the reflection direction R,
        // at the mip level that corresponds to roughness
        vec3  R           = reflect(-V, N);
        float lod         = roughness * float(IBL_MIPS - 1);
        vec3  prefiltered = textureLod(u_PrefilteredEnvMap, equirectUV(R), lod).rgb;

        // BRDF LUT encodes the split-sum integral: (F0_scale, F0_bias)
        vec2  brdf        = texture(u_BrdfLut, vec2(NdotV, roughness)).rg;
        vec3  specular    = prefiltered * (F0 * brdf.x + brdf.y);

        ambient = (diffuse + specular) * ao;
    } else {
        // Flat ambient fallback when no IBL textures are available
        ambient = 0.03 * albedo * ao;
    }

    vec3 color = Lo + ambient;
    color += albedo * mat.emissive;

    // HDR linear output — ToneMappingPass applies the operator and gamma.
    fragColor = vec4(color * exposure, 1.0);
}
