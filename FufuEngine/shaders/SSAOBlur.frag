#version 430 core

// Box blur 4x4 sur la texture d'occlusion R8 pour lisser le bruit du kernel.

in  vec2 v_UV;
out float fragAO;

uniform sampler2D u_AO;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(u_AO, 0));
    float result = 0.0;
    for (int x = -2; x <= 1; ++x)
        for (int y = -2; y <= 1; ++y)
            result += texture(u_AO, v_UV + vec2(x, y) * texelSize).r;
    fragAO = result / 16.0;
}
