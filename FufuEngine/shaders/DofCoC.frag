#version 430 core

// Calcule le Circle of Confusion normalisé [0=net, 1=flou_max] pour chaque
// fragment à partir de la distance caméra-fragment et du plan focal.

in  vec2 v_UV;
out float fragCoC;

uniform sampler2D u_GPosition;  // world-space positions (gPos.w >= 0.5 = géométrie)
uniform vec3  u_CamPos;
uniform float u_FocusDist;      // distance au plan de netteté (world units)
uniform float u_FocusRange;     // demi-largeur de la zone nette (world units)

void main()
{
    vec4 gPos = texture(u_GPosition, v_UV);
    if (gPos.w < 0.5) { fragCoC = 0.0; return; }   // ciel → toujours net

    float dist = length(gPos.xyz - u_CamPos);
    fragCoC    = clamp(abs(dist - u_FocusDist) / max(u_FocusRange, 0.001), 0.0, 1.0);
}
