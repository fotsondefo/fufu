#version 430 core

// Additif : HDR original + bloom flou * strength.
// Sortie toujours HDR linéaire, consommée par ToneMappingPass.

in  vec2 v_UV;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D u_HDR;
layout(binding = 1) uniform sampler2D u_Bloom;
uniform float u_Strength;

void main() {
    vec3 hdr   = texture(u_HDR,   v_UV).rgb;
    vec3 bloom = texture(u_Bloom, v_UV).rgb;
    fragColor  = vec4(hdr + bloom * u_Strength, 1.0);
}
