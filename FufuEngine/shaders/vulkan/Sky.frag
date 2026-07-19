#version 450

// Binding remapping : UBO=slot, sampler=slot+32
// u_Skybox GL(binding=0 sampler) → VK(binding=32)
// FrameBlock GL(binding=0 UBO)   → VK(binding=0)

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0, std140) uniform FrameBlock {
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

layout(set = 0, binding = 32) uniform sampler2D u_Skybox;

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
    gl_FragDepth = 1.0;
}
