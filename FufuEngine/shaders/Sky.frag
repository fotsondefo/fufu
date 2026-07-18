#version 430 core

// Ciel fullscreen : procédural ou HDRI. Utilisé par ForwardPass avant la géométrie
// (gl_FragDepth = 1.0) et par DeferredLighting pour les pixels sans géométrie.
// FullscreenQuad.vert fournit v_UV [0,1]x[0,1].

in  vec2 v_UV;
out vec4 fragColor;

layout(std140, binding = 0) uniform FrameBlock {
    mat4  viewProj;
    vec3  camPos;      float _p0;
    vec3  camForward;  float camFov;
    vec3  camRight;    float camAspect;
    vec3  camUp;       float exposure;
    int   lightCount;
    int   hasSkybox;
    float skyboxIntensity;
    float _p1;
};

layout(binding = 0) uniform sampler2D u_Skybox;

const float PI = 3.14159265359;

vec3 sampleSky(vec3 dir) {
    if (hasSkybox == 1) {
        float u = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
        float v = acos(clamp(dir.y, -1.0, 1.0)) / PI;
        return texture(u_Skybox, vec2(u, v)).rgb * skyboxIntensity;
    }
    float t = 0.5 * (dir.y + 1.0);
    return mix(vec3(1.0), vec3(0.5, 0.7, 1.0), t);
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2  ndc   = v_UV * 2.0 - 1.0;
    float scale = tan(camFov * 0.5);
    vec3  dir   = normalize(camForward
                          + ndc.x * camAspect * scale * camRight
                          + ndc.y             * scale * camUp);

    vec3 color = sampleSky(dir) * exposure;
    color = aces(color);
    color = pow(color, vec3(1.0 / 2.2));

    fragColor    = vec4(color, 1.0);
    gl_FragDepth = 1.0; // toujours derrière la géométrie rasterisée
}
