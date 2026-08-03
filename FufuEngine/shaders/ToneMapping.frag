#version 430 core

// Tone mapping post-process : lit une texture HDR linéaire et applique
// l'opérateur sélectionné + correction gamma. Reçoit FullscreenQuad.vert.
// Opérateurs : 0=None, 1=Reinhard, 2=ACES, 3=Filmic/Uncharted2.
// Filmic a le gamma intégré — on saute la correction explicite pour lui.

in  vec2 v_UV;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D u_Source;

uniform int   u_Operator;
uniform float u_Gamma;

vec3 reinhard(vec3 x) {
    return x / (1.0 + x);
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// Uncharted 2 — gamma ~2.2 intégré dans la courbe, pas besoin de pow() après.
vec3 filmic(vec3 x) {
    vec3 X = max(vec3(0.0), x - 0.004);
    return (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
}

void main() {
    vec3 color = texture(u_Source, v_UV).rgb;

    if (u_Operator == 1)
        color = reinhard(color);
    else if (u_Operator == 2)
        color = aces(color);
    else if (u_Operator == 3) {
        fragColor = vec4(filmic(color), 1.0);
        return;
    }

    fragColor = vec4(pow(max(color, vec3(0.0)), vec3(1.0 / u_Gamma)), 1.0);
}
