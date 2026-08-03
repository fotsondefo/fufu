#version 430 core

// Flou gaussien séparable 9 taps (sigma=2).
// Utilisé en deux passes : u_Direction=(1,0) puis (0,1).

in  vec2 v_UV;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D u_Source;
uniform vec2 u_Direction;   // (1,0) horizontal  |  (0,1) vertical
uniform vec2 u_TexelSize;   // 1.0 / vec2(width, height)

// Poids gaussiens normalisés (sigma=2, taps aux offsets 0..4)
const float w[5] = float[](0.2041, 0.1801, 0.1238, 0.0663, 0.0277);

void main() {
    vec3 result = texture(u_Source, v_UV).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        vec2 off = u_Direction * u_TexelSize * float(i);
        result  += texture(u_Source, v_UV + off).rgb * w[i];
        result  += texture(u_Source, v_UV - off).rgb * w[i];
    }
    fragColor = vec4(result, 1.0);
}
