#version 430 core

// Flou bokeh par disque de Vogel (spirale angle d'or).
// Rayon = cocValue * u_MaxBlurRadius pixels.
// Rotation aléatoire par pixel (hash) pour casser le motif en spirale.

in  vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_HDR;
uniform sampler2D u_CoC;
uniform vec2  u_TexelSize;      // 1.0 / vec2(width, height)
uniform float u_MaxBlurRadius;  // rayon maximal en pixels
uniform int   u_Samples;        // ≤ 32

float hash21(vec2 p)
{
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}

void main()
{
    float coc = texture(u_CoC, v_UV).r;

    if (coc < 0.001)
    {
        fragColor = texture(u_HDR, v_UV);
        return;
    }

    float radius = coc * u_MaxBlurRadius;
    float phi    = hash21(v_UV) * 6.28318;   // rotation aléatoire per-pixel

    int   n         = min(u_Samples, 32);
    vec4  color     = vec4(0.0);
    float weightSum = 0.0;

    for (int i = 0; i < n; ++i)
    {
        // Disque de Vogel : distribution quasi-uniforme
        float r     = sqrt(float(i) + 0.5) / sqrt(float(n));
        float theta = float(i) * 2.39996 + phi;  // angle d'or ≈ 2.39996 rad

        vec2 offset = vec2(cos(theta), sin(theta)) * r * radius * u_TexelSize;

        // Poids par CoC du voisin : seuls les fragments flous contribuent au flou
        float sampleCoC = texture(u_CoC, v_UV + offset).r;
        float w = mix(coc, sampleCoC, 0.5);   // évite les halos sur les bords nets

        color      += texture(u_HDR, v_UV + offset) * w;
        weightSum  += w;
    }

    fragColor = (weightSum > 0.0) ? color / weightSum : texture(u_HDR, v_UV);
}
