#version 430 core

// Extrait les pixels dont la luminance dépasse le seuil.
// Soft-knee : transition douce autour du threshold pour éviter
// le halo dur sur les bords de highlights.

in  vec2 v_UV;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D u_Source;
uniform float u_Threshold;
uniform float u_Knee;

void main() {
    vec3  color      = texture(u_Source, v_UV).rgb;
    float brightness = max(color.r, max(color.g, color.b));

    // Soft-knee : courbe quadratique autour du seuil.
    float rq     = clamp(brightness - u_Threshold + u_Knee, 0.0, 2.0 * u_Knee);
    rq           = (rq * rq) / (4.0 * u_Knee + 1e-5);
    float weight = max(rq, brightness - u_Threshold) / max(brightness, 1e-5);

    fragColor = vec4(color * weight, 1.0);
}
