#version 430 core

in vec3 v_WorldPos;
in vec3 v_WorldNormal;
in vec2 v_UV;
flat in int v_MaterialIndex;

layout(location = 0) out vec4 gPosition; // xyz=worldPos, w=1.0 (w<0.5 signifie ciel)
layout(location = 1) out vec4 gNormal;   // xyz=normale, w=matIdx (float)
layout(location = 2) out vec4 gUV;       // xy=UV

void main() {
    gPosition = vec4(v_WorldPos, 1.0);
    gNormal   = vec4(normalize(v_WorldNormal), float(v_MaterialIndex));
    gUV       = vec4(v_UV, 0.0, 0.0);
}
