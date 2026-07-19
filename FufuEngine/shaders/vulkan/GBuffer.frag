#version 450

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_WorldNormal;
layout(location = 2) in vec2 v_UV;
layout(location = 3) flat in int v_MaterialIndex;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gUV;

void main() {
    gPosition = vec4(v_WorldPos, 1.0);
    gNormal   = vec4(normalize(v_WorldNormal), float(v_MaterialIndex));
    gUV       = vec4(v_UV, 0.0, 0.0);
}
