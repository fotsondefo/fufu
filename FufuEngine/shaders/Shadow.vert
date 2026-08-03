#version 430 core

// Rendu de la shadow map : vertex-pulling depuis PositionBuffer (binding 2),
// pas de VAO/VBO. Même schéma que GBuffer.vert mais sans attributs de surface.

struct TriPos {
    vec4 v0, v1, v2;
};

layout(std430, binding = 2) readonly buffer PositionBuffer { TriPos positions[]; };

layout(std140, binding = 0) uniform ShadowBlock {
    mat4 lightSpaceMatrix;
};

layout(std140, binding = 1) uniform DrawBlock {
    mat4 transform;
    mat4 invTransform;
    int  triOffset;
    int  materialIndex;
    int  _pd[2];
};

void main() {
    int triIdx = triOffset + gl_VertexID / 3;
    int v      = gl_VertexID % 3;

    vec3 localPos;
    if      (v == 0) localPos = positions[triIdx].v0.xyz;
    else if (v == 1) localPos = positions[triIdx].v1.xyz;
    else             localPos = positions[triIdx].v2.xyz;

    gl_Position = lightSpaceMatrix * transform * vec4(localPos, 1.0);
}
