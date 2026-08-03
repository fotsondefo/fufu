#version 430 core

// Pre-computes the split-sum BRDF lookup table (Brian Karis 2013).
// Input UV: x = NdotV, y = roughness.
// Output RG: (F0 scale, F0 bias) used in the specular IBL term:
//   specularIBL = prefilteredColor * (F0 * brdf.r + brdf.g)

in  vec2 v_UV;
out vec2 fragColor;

const float PI = 3.14159265359;

float radicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radicalInverseVdC(i));
}

vec3 importanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;

    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / max(1.0 + (a2 - 1.0) * Xi.y, 1e-6));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    vec3 up    = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);

    return normalize(right * H.x + up * H.y + N * H.z);
}

// Geometry term with IBL roughness remapping (k = roughness² / 2, not (r+1)²/8).
float geometrySchlickGGX_IBL(float NdotX, float roughness) {
    float k = (roughness * roughness) / 2.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-6);
}

void main() {
    float NdotV     = max(v_UV.x, 1e-4);
    float roughness = v_UV.y;

    // Construct a view vector with the given NdotV.
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0); // integrate in tangent space

    float scale = 0.0;
    float bias  = 0.0;

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importanceSampleGGX(Xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z,        0.0);
        float NdotH = max(H.z,        0.0);
        float VdotH = max(dot(V, H),  0.0);

        if (NdotL > 0.0) {
            float G     = geometrySchlickGGX_IBL(NdotV, roughness)
                        * geometrySchlickGGX_IBL(NdotL, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 1e-6);
            float Fc    = pow(1.0 - VdotH, 5.0);
            scale += (1.0 - Fc) * G_Vis;
            bias  +=        Fc  * G_Vis;
        }
    }

    fragColor = vec2(scale, bias) / float(SAMPLE_COUNT);
}
