#version 450

// Binding remapping : UBO=slot, SSBO=slot+16, sampler=slot+32
// MaterialBuffer  GL(binding=3)  → VK(binding=19)
// LightBuffer     GL(binding=9)  → VK(binding=25)
// u_Skybox        GL(binding=0)  → VK(binding=32)
// u_MaterialTextures GL(binding=1) → VK(binding=33)
// u_GBuffer_Position GL(binding=17) → VK(binding=49)
// u_GBuffer_Normal   GL(binding=18) → VK(binding=50)
// u_GBuffer_UV       GL(binding=19) → VK(binding=51)

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

struct Material {
    vec4  albedo;
    float metallic;
    float roughness;
    float emissive;
    float ior;
    int   albedoTexIdx;
    float _pad0, _pad1, _pad2;
};

struct Light {
    vec4  positionOrDir;
    vec4  color;
    float radius;
    int   type;
    float _pad[2];
};

layout(set = 0, binding = 19, std430) readonly buffer MaterialBuffer { Material materials[]; };
layout(set = 0, binding = 25, std430) readonly buffer LightBuffer    { Light lights[]; };

layout(set = 0, binding = 32) uniform sampler2D u_Skybox;
layout(set = 0, binding = 33) uniform sampler2D u_MaterialTextures[16];
layout(set = 0, binding = 49) uniform sampler2D u_GBuffer_Position;
layout(set = 0, binding = 50) uniform sampler2D u_GBuffer_Normal;
layout(set = 0, binding = 51) uniform sampler2D u_GBuffer_UV;

layout(set = 0, binding = 0, std140) uniform FrameBlock {
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

vec3 sampleSky(vec3 dir) {
    if (hasSkybox == 1) {
        float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
        float v = acos(clamp(dir.y, -1.0, 1.0)) / PI;
        return texture(u_Skybox, vec2(u, v)).rgb * skyboxIntensity;
    }
    float t = 0.5 * (dir.y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

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
    vec4 gPos = texture(u_GBuffer_Position, v_UV);

    if (gPos.w < 0.5) {
        vec2  ndc   = v_UV * 2.0 - 1.0;
        float scale = tan(camFov * 0.5);
        vec3  dir   = normalize(camForward
                              + ndc.x * camAspect * scale * camRight
                              + ndc.y             * scale * camUp);
        vec3 sky = sampleSky(dir) * exposure;
        sky = aces(sky);
        sky = pow(sky, vec3(1.0 / 2.2));
        fragColor = vec4(sky, 1.0);
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

    vec3 V     = normalize(camPos - worldPos);
    vec3 color = vec3(0.0);

    for (int i = 0; i < lightCount; ++i) {
        Light light = lights[i];
        vec3 L;
        vec3 radiance;

        if (light.type == 0) {
            L        = normalize(light.positionOrDir.xyz);
            radiance = light.color.rgb * light.color.a;
        } else {
            vec3  toLight     = light.positionOrDir.xyz - worldPos;
            float dist        = length(toLight);
            L                 = toLight / dist;
            float attenuation = 1.0 / max(dist * dist, 0.01);
            radiance          = light.color.rgb * light.color.a * attenuation;
        }

        color += pbrLight(N, V, L, albedo, mat.metallic, mat.roughness, radiance);
    }

    color += 0.03 * albedo;
    color += albedo * mat.emissive;

    color = aces(color * exposure);
    color = pow(color, vec3(1.0 / 2.2));

    fragColor = vec4(color, 1.0);
}
