#version 430 core

out vec4 fragColor;
in  vec2 v_UV;

// ── Input textures ──────────────────────────────────────────────────────────
uniform sampler2D       u_HdrTex;
uniform sampler2D       u_GPosTex;
uniform sampler2DShadow u_ShadowTex;

// Up to 4 density textures (GL_TEXTURE_3D), one per volume slot.
// Indexed statically via if-chain for portability across GL drivers.
uniform sampler3D u_DensityTex0;
uniform sampler3D u_DensityTex1;
uniform sampler3D u_DensityTex2;
uniform sampler3D u_DensityTex3;

// ── Camera ───────────────────────────────────────────────────────────────────
uniform vec3  u_CamPos;
uniform vec3  u_CamForward;
uniform vec3  u_CamRight;
uniform vec3  u_CamUp;
uniform float u_CamFov;
uniform float u_CamAspect;

// ── Shadow ───────────────────────────────────────────────────────────────────
uniform mat4  u_LightSpaceMatrix;
uniform float u_ShadowBias;
uniform int   u_ShadowEnabled;

// ── Light ────────────────────────────────────────────────────────────────────
uniform vec3  u_LightDir;
uniform vec3  u_LightColor;
uniform float u_LightIntensity;

// ── Per-volume UBO (binding = 5) ─────────────────────────────────────────────
struct VolumeData {
    vec4 boundsMin;   // xyz = worldMin,  w = density
    vec4 boundsMax;   // xyz = worldMax,  w = scattering
    vec4 albedo;      // xyz = albedo,    w = absorption
    vec4 emission;    // xyz = emColor,   w = emissionStrength
    vec4 params;      // x = anisotropy,  y = marchSteps (as float)
};

layout(std140, binding = 5) uniform VolumeBlock {
    VolumeData u_Volumes[4];
    int        u_VolumeCount;
    int        _pad0, _pad1, _pad2;
};

// ── Helpers ──────────────────────────────────────────────────────────────────

bool rayAABB(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax,
             out float tMin, out float tMax)
{
    vec3 invD  = 1.0 / rd;
    vec3 t0    = (bmin - ro) * invD;
    vec3 t1    = (bmax - ro) * invD;
    vec3 tNear = min(t0, t1);
    vec3 tFar  = max(t0, t1);
    tMin = max(max(tNear.x, tNear.y), tNear.z);
    tMax = min(min(tFar.x,  tFar.y),  tFar.z);
    return tMax > max(tMin, 0.0);
}

float hg(float cosTheta, float g) {
    float g2  = g * g;
    float den = max(1.0 + g2 - 2.0 * g * cosTheta, 0.001);
    return (1.0 - g2) / (4.0 * 3.14159265359 * pow(den, 1.5));
}

float shadowLookup(vec3 worldPos) {
    if (u_ShadowEnabled == 0) return 1.0;
    vec4 lsPos = u_LightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 proj  = lsPos.xyz / lsPos.w * 0.5 + 0.5;
    if (any(lessThan(proj.xy, vec2(0.0))) || any(greaterThan(proj.xy, vec2(1.0))) || proj.z > 1.0)
        return 1.0;
    return texture(u_ShadowTex, vec3(proj.xy, proj.z - u_ShadowBias));
}

float sampleDensity(int vi, vec3 uvw) {
    if (vi == 0) return texture(u_DensityTex0, uvw).r;
    if (vi == 1) return texture(u_DensityTex1, uvw).r;
    if (vi == 2) return texture(u_DensityTex2, uvw).r;
                 return texture(u_DensityTex3, uvw).r;
}

// ── Main ─────────────────────────────────────────────────────────────────────

void main()
{
    vec4 hdrIn = texture(u_HdrTex, v_UV);

    if (u_VolumeCount == 0) {
        fragColor = hdrIn;
        return;
    }

    // Reconstruct camera ray from UV
    float tanH = tan(u_CamFov * 0.5);
    vec2  ndc  = v_UV * 2.0 - 1.0;
    vec3  rd   = normalize(u_CamForward
                           + ndc.x * tanH * u_CamAspect * u_CamRight
                           + ndc.y * tanH * u_CamUp);

    // Ray termination at geometry
    vec4  gPos     = texture(u_GPosTex, v_UV);
    float geomDist = (gPos.a > 0.5) ? length(gPos.xyz - u_CamPos) : 1e30;

    float cosTheta      = dot(rd, u_LightDir);
    vec3  totalScatter  = vec3(0.0);
    float totalTransmit = 1.0;

    for (int vi = 0; vi < u_VolumeCount && vi < 4; ++vi)
    {
        vec3 bmin = u_Volumes[vi].boundsMin.xyz;
        vec3 bmax = u_Volumes[vi].boundsMax.xyz;

        float tMin, tMax;
        if (!rayAABB(u_CamPos, rd, bmin, bmax, tMin, tMax)) continue;

        tMin = max(tMin, 0.001);
        tMax = min(tMax, geomDist);
        if (tMin >= tMax) continue;

        float density    = u_Volumes[vi].boundsMin.w;
        float scattering = u_Volumes[vi].boundsMax.w;
        float absorption = u_Volumes[vi].albedo.w;
        vec3  albedo     = u_Volumes[vi].albedo.xyz;
        vec3  emColor    = u_Volumes[vi].emission.xyz;
        float emStrength = u_Volumes[vi].emission.w;
        float g          = u_Volumes[vi].params.x;
        int   steps      = clamp(int(u_Volumes[vi].params.y), 4, 256);

        float stepLen = (tMax - tMin) / float(steps);
        float phase   = hg(cosTheta, g);
        vec3  invSize = 1.0 / (bmax - bmin);

        vec3  volScatter = vec3(0.0);
        float T          = 1.0;

        for (int i = 0; i < steps; ++i)
        {
            float t   = tMin + (float(i) + 0.5) * stepLen;
            vec3  p   = u_CamPos + rd * t;
            vec3  uvw = clamp((p - bmin) * invSize, 0.0, 1.0);

            float d = sampleDensity(vi, uvw) * density;
            if (d < 0.001) continue;

            float sigma_t = d * (scattering + absorption);
            T *= exp(-sigma_t * stepLen);

            float vis = shadowLookup(p);
            volScatter += T * d * scattering * albedo
                        * u_LightColor * u_LightIntensity * vis * phase * stepLen;

            if (emStrength > 0.0)
                volScatter += T * d * emColor * emStrength * stepLen;

            if (T < 0.001) { T = 0.0; break; }
        }

        totalScatter  += volScatter * totalTransmit;
        totalTransmit *= T;
    }

    fragColor = vec4(hdrIn.rgb * totalTransmit + totalScatter, hdrIn.a);
}
