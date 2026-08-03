#version 430 core

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_UV;
flat in int v_MaterialIndex;

layout(location = 0) out vec4 gPosition; // xyz=worldPos, w=1.0 (w<0.5 means sky)
layout(location = 1) out vec4 gNormal;   // xyz=normal (world-space, may be normal-mapped), w=matIdx
layout(location = 2) out vec4 gUV;       // xy=UV

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

layout(std430, binding = 3) readonly buffer MaterialBuffer { Material materials[]; };
layout(binding = 1) uniform sampler2D u_MaterialTextures[16];

void main() {
    vec3 N = normalize(v_WorldNormal);

    Material mat = materials[v_MaterialIndex];

    if (mat.normalTexIdx >= 0) {
        // Cotangent-frame TBN from screen-space derivatives — no tangent attribute needed.
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

    gPosition = vec4(v_WorldPos, 1.0);
    gNormal   = vec4(N, float(v_MaterialIndex));
    gUV       = vec4(v_UV, 0.0, 0.0);
}
