#version 430 core

// SSAO : sampling hémisphérique en view-space.
// Inputs : positions/normales world-space du G-Buffer, texture de bruit 4x4.
// Output : R = facteur d'occlusion [0=occlus, 1=libre].

in  vec2 v_UV;
out float fragAO;

uniform sampler2D u_GPosition;   // binding 0 — world-space positions (gPos.w >= 0.5 = géométrie)
uniform sampler2D u_GNormal;     // binding 1 — world-space normales
uniform sampler2D u_Noise;       // binding 2 — 4x4 RG16F rotation aléatoire

uniform mat4  u_View;
uniform mat4  u_Proj;
uniform float u_Radius;          // rayon de sampling (unités view-space)
uniform float u_Bias;            // décalage pour éviter l'auto-occlusion
uniform float u_Strength;        // multiplicateur final
uniform int   u_NumSamples;      // ≤ 64
uniform vec2  u_ScreenSize;      // largeur x hauteur (pixels)
uniform vec3  u_Kernel[64];      // échantillons hémisphériques normalisés

void main()
{
    vec4 gPos = texture(u_GPosition, v_UV);
    if (gPos.w < 0.5) { fragAO = 1.0; return; }   // fragment ciel → pas d'occlusion

    vec2  noiseScale  = u_ScreenSize / 4.0;
    vec3  randVec     = normalize(vec3(texture(u_Noise, v_UV * noiseScale).rg, 0.0));

    // Passage en view-space
    vec3 fragPos = vec3(u_View * vec4(gPos.xyz, 1.0));
    vec3 normal  = normalize(mat3(u_View) * texture(u_GNormal, v_UV).xyz);

    // TBN pour orienter le kernel autour de la normale
    vec3 tangent  = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitang   = cross(normal, tangent);
    mat3 TBN      = mat3(tangent, bitang, normal);

    float occlusion = 0.0;
    int   n         = min(u_NumSamples, 64);

    for (int i = 0; i < n; ++i)
    {
        vec3 samplePos = TBN * u_Kernel[i];
        samplePos      = fragPos + samplePos * u_Radius;

        // Projection vers l'espace écran
        vec4 offset = u_Proj * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        // Profondeur de référence au point d'échantillon
        vec4 refPos = texture(u_GPosition, offset.xy);
        if (refPos.w < 0.5) continue;           // ciel → pas d'occlusion
        float refDepth = (u_View * vec4(refPos.xyz, 1.0)).z;

        // Range check : si trop loin en profondeur, on ignore (évite les halos)
        float rangeCheck = smoothstep(0.0, 1.0, u_Radius / abs(fragPos.z - refDepth));
        occlusion += (refDepth >= samplePos.z + u_Bias ? 1.0 : 0.0) * rangeCheck;
    }

    fragAO = 1.0 - clamp((occlusion / float(n)) * u_Strength, 0.0, 1.0);
}
