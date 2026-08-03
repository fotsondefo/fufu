#version 430 core

// Pre-filters an equirectangular environment map for specular IBL using GGX
// importance sampling (split-sum approximation, Brian Karis 2013).
// Rendered once per roughness level (mip), with u_Roughness in [0, 1].

in  vec2 v_UV;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D u_Env;
uniform float u_Roughness;

const float PI = 3.14159265359;

vec2 equirectUV(vec3 dir) {
    return vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5,
                acos(clamp(dir.y, -1.0, 1.0)) / PI);
}

vec3 uvToDir(vec2 uv) {
    float phi   = (uv.x - 0.5) * 2.0 * PI;
    float theta = uv.y * PI;
    return vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
}

// Van der Corput radical inverse — maps an integer to a float in [0,1).
float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

// Hammersley 2D low-discrepancy sequence.
vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radicalInverseVdC(i));
}

// GGX importance sample: generates a half-vector H distributed according to
// the GGX NDF around N.
vec3 importanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;

    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / max(1.0 + (a2 - 1.0) * Xi.y, 1e-6));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Tangent-space H
    vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Rotate to world space around N
    vec3 up    = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);

    return normalize(right * H.x + up * H.y + N * H.z);
}

void main() {
    vec3 N = normalize(uvToDir(v_UV));
    // Isotropic assumption: V == N == R (view direction equals reflection axis).
    vec3 V = N;

    const uint SAMPLE_COUNT  = 512u;
    vec3  prefilteredColor   = vec3(0.0);
    float totalWeight        = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importanceSampleGGX(Xi, N, u_Roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            prefilteredColor += texture(u_Env, equirectUV(L)).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }

    fragColor = vec4(prefilteredColor / max(totalWeight, 1e-4), 1.0);
}
