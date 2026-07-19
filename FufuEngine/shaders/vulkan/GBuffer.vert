#version 450

// Binding remapping : UBO=slot, SSBO=slot+16, sampler=slot+32
// PositionBuffer GL(binding=2)  → VK(binding=18)
// AttributeBuffer GL(binding=10) → VK(binding=26)

struct TriPos {
    vec4 v0, v1, v2;
};

struct TriAttr {
    vec4 n0, n1, n2;
    vec2 uv0, uv1, uv2;
    int  materialIndex;
    float _pad;
};

layout(set = 0, binding = 18, std430) readonly buffer PositionBuffer  { TriPos positions[]; };
layout(set = 0, binding = 26, std430) readonly buffer AttributeBuffer { TriAttr attributes[]; };

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

layout(set = 0, binding = 1, std140) uniform DrawBlock {
    mat4 transform;
    mat4 invTransform;
    int  triOffset;
    int  materialIndex;
    int  _pd[2];
};

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_WorldNormal;
layout(location = 2) out vec2 v_UV;
layout(location = 3) flat out int v_MaterialIndex;

void main() {
    int triIdx = triOffset + gl_VertexIndex / 3;
    int v      = gl_VertexIndex % 3;

    vec3 localPos;
    vec3 localNrm;
    if (v == 0) {
        localPos = positions[triIdx].v0.xyz;
        localNrm = attributes[triIdx].n0.xyz;
        v_UV     = attributes[triIdx].uv0;
    } else if (v == 1) {
        localPos = positions[triIdx].v1.xyz;
        localNrm = attributes[triIdx].n1.xyz;
        v_UV     = attributes[triIdx].uv1;
    } else {
        localPos = positions[triIdx].v2.xyz;
        localNrm = attributes[triIdx].n2.xyz;
        v_UV     = attributes[triIdx].uv2;
    }

    vec4 worldPos  = transform * vec4(localPos, 1.0);
    v_WorldPos     = worldPos.xyz;
    v_WorldNormal  = normalize(transpose(mat3(invTransform)) * localNrm);
    v_MaterialIndex = materialIndex;

    gl_Position = viewProj * worldPos;
}
