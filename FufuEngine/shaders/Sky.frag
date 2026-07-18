#version 430 core

// Ciel fullscreen : procédural ou HDRI. Utilisé par ForwardPass avant la géométrie
// (gl_FragDepth = 1.0) et par DeferredLighting pour les pixels sans géométrie.
// FullscreenQuad.vert fournit v_UV [0,1]x[0,1].

in  vec2 v_UV;
out vec4 fragColor;

uniform vec3  u_CamForward;
uniform vec3  u_CamRight;
uniform vec3  u_CamUp;
uniform float u_CamFov;     // radians
uniform float u_CamAspect;
uniform int   u_HasSkybox;
uniform float u_SkyboxIntensity;
uniform float u_Exposure;

layout(binding = 0) uniform sampler2D u_Skybox;

const float PI = 3.14159265359;

vec3 sampleSky(vec3 dir) {
    if (u_HasSkybox == 1) {
        float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
        float v = acos(clamp(dir.y, -1.0, 1.0)) / PI;
        return texture(u_Skybox, vec2(u, v)).rgb * u_SkyboxIntensity;
    }
    float t = 0.5 * (dir.y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2  ndc   = v_UV * 2.0 - 1.0;
    float scale = tan(u_CamFov * 0.5);
    vec3  dir   = normalize(u_CamForward
                          + ndc.x * u_CamAspect * scale * u_CamRight
                          + ndc.y               * scale * u_CamUp);

    vec3 color = sampleSky(dir) * u_Exposure;
    color = aces(color);
    color = pow(color, vec3(1.0 / 2.2));

    fragColor    = vec4(color, 1.0);
    gl_FragDepth = 1.0; // toujours derrière la géométrie rasterisée
}
