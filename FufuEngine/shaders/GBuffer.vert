#version 430 core

// Fetch des sommets depuis les SSBOs via gl_VertexID — pas de VAO/VBO.
// triOffset + gl_VertexID/3 = index absolu dans le buffer BLAS concaténé.

struct TriPos {
    vec4 v0, v1, v2;
};

struct TriAttr {
    vec4 n0, n1, n2;
    vec2 uv0, uv1, uv2;
    int  materialIndex;
    float _pad;
};

layout(std430, binding = 2)  readonly buffer PositionBuffer  { TriPos positions[]; };
layout(std430, binding = 10) readonly buffer AttributeBuffer { TriAttr attributes[]; };

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

layout(std140, binding = 1) uniform DrawBlock {
    mat4 transform;
    mat4 invTransform;
    int  triOffset;
    int  materialIndex;
    int  _pd[2];
};

out vec3 v_WorldPos;
out vec3 v_WorldNormal;
out vec2 v_UV;
flat out int v_MaterialIndex;

void main() {
    int triIdx = triOffset + gl_VertexID / 3;
    int v      = gl_VertexID % 3;

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
