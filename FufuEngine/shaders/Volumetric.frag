#version 430 core

// Volumetric light scattering par raymarching à demi-résolution.
// Sortie : vec4(inscatter.rgb, transmittance) en RGBA16F.
// Composite dans VolumetricComposite.frag.

in  vec2 v_UV;
out vec4 fragScatter;

uniform sampler2D       u_GPosition;   // world-space positions (gPos.w >= 0.5 = géo)
uniform sampler2DShadow u_ShadowMap;   // depth compare

// Caméra
uniform vec3  u_CamPos;
uniform vec3  u_CamForward;
uniform vec3  u_CamRight;
uniform vec3  u_CamUp;
uniform float u_CamFov;
uniform float u_CamAspect;

// Lumière directionnelle
uniform vec3  u_LightDir;       // FROM surface TO light (normalisé)
uniform vec3  u_LightColor;
uniform float u_LightIntensity;
uniform mat4  u_LightSpaceMatrix;
uniform float u_ShadowBias;
uniform int   u_ShadowEnabled;

// Paramètres du volume
uniform int   u_Steps;
uniform float u_Density;        // σ_t : extinction
uniform float u_Scattering;     // albedo σ_s / σ_t
uniform float u_Anisotropy;     // HG g ∈ [-1, 1]
uniform float u_Ambient;        // lumière ambiante dans le volume
uniform float u_MaxDist;        // distance max pour les rayons ciel

const float PI = 3.14159265359;

// Henyey-Greenstein : g > 0 = forward scatter (rayons du soleil)
float phaseHG(float cosTheta, float g)
{
    float g2  = g * g;
    float den = max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4);
    return (1.0 - g2) / (4.0 * PI * pow(den, 1.5));
}

// Visibilité dans la shadow map (1 = éclairé, 0 = ombre)
float sampleShadow(vec3 worldPos)
{
    if (u_ShadowEnabled == 0) return 1.0;
    vec4 posLS     = u_LightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = posLS.xyz / posLS.w * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 1.0;
    if (any(lessThan(projCoords.xy, vec2(0.0)))) return 1.0;
    if (any(greaterThan(projCoords.xy, vec2(1.0)))) return 1.0;
    return texture(u_ShadowMap, vec3(projCoords.xy, projCoords.z - u_ShadowBias));
}

// Hash per-pixel pour le jitter (casse le banding en bruit)
float hash21(vec2 p)
{
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}

void main()
{
    // Reconstruction de la direction du rayon
    vec2  ndc   = v_UV * 2.0 - 1.0;
    float scale = tan(u_CamFov * 0.5);
    vec3  rayDir = normalize(u_CamForward
                           + ndc.x * u_CamAspect * scale * u_CamRight
                           + ndc.y             * scale * u_CamUp);

    // Longueur du rayon : position du fragment ou distance max pour le ciel
    vec4  gPos   = texture(u_GPosition, v_UV);
    bool  isSky  = (gPos.w < 0.5);
    float maxDist = isSky
                  ? u_MaxDist
                  : length(gPos.xyz - u_CamPos);

    int   n         = max(u_Steps, 1);
    float stepSize  = maxDist / float(n);
    float jitter    = hash21(v_UV) * stepSize;   // décalage aléatoire du premier sample

    float cosTheta = dot(rayDir, u_LightDir);
    float phase    = phaseHG(cosTheta, u_Anisotropy);

    vec3  inscatter    = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < n; ++i)
    {
        float t      = jitter + float(i) * stepSize;
        vec3  pos    = u_CamPos + rayDir * t;

        float vis    = sampleShadow(pos);

        float sigma_t = u_Density;
        float sigma_s = sigma_t * u_Scattering;

        // In-scattering de la lumière directionnelle + ambiant
        vec3 Li = u_LightColor * (u_LightIntensity * vis * phase) + vec3(u_Ambient);

        // Intégrale analytique sur le pas : évite la sur-estimation pour les grands pas
        float stepTrans = exp(-sigma_t * stepSize);
        inscatter      += transmittance * sigma_s * Li * (1.0 - stepTrans) / max(sigma_t, 1e-6);
        transmittance  *= stepTrans;

        if (transmittance < 0.005) break;  // sortie anticipée
    }

    fragScatter = vec4(inscatter, transmittance);
}
