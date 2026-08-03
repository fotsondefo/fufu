#version 430 core

in  vec2  v_UV;
in  vec3  v_Color;
in  float v_Opacity;
in  float v_CamDepth;
in  float v_CA, v_CB, v_CC;

uniform sampler2D u_GPosTex;    // world-space positions from GBuffer
uniform vec2      u_ScreenSize;
uniform vec3      u_CamPos;
uniform mat4      u_View;

out vec4 fragColor;

void main()
{
    // Evaluate Gaussian: power = 0.5 * d^T * Sigma_inv * d
    // where Sigma_inv = [ a  b; b  c ] (the conic matrix)
    float dx = v_UV.x, dy = v_UV.y;
    float power = -0.5 * (v_CA * dx*dx + 2.0*v_CB * dx*dy + v_CC * dy*dy);
    if (power > 0.0) discard;  // outside valid Gaussian region

    float alpha = v_Opacity * exp(power);
    if (alpha < 1.0 / 255.0) discard;

    // Depth test against scene geometry (sample gPos at this fragment's screen pos)
    vec2 uv = gl_FragCoord.xy / u_ScreenSize;
    vec4 gPos = texture(u_GPosTex, uv);
    if (gPos.w > 0.5) // w > 0 means valid geometry was written
    {
        // Convert world-space geometry position to camera depth
        vec4 camSpaceGeo = u_View * vec4(gPos.xyz, 1.0);
        float sceneDepth = -camSpaceGeo.z;  // camera looks down -Z
        if (v_CamDepth > sceneDepth)
            discard;  // splat is behind opaque geometry
    }

    // Premultiplied alpha output — blend with GL_ONE, GL_ONE_MINUS_SRC_ALPHA
    fragColor = vec4(v_Color * alpha, alpha);
}
