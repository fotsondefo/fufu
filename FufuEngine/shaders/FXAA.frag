#version 430 core
// FXAA — post-process par détection de contraste de luminance.
// Version simplifiée "maison" (pas le FXAA 3.11 de NVIDIA) : détecte les
// bords via le contraste de luminance entre voisins directs, puis floute
// le long de la direction dominante du gradient (approximée via les 4
// voisins diagonaux) proportionnellement à ce contraste. Moins précis que
// l'algorithme complet mais bien moins de code, et suffisant pour lisser
// les crénelures d'un rendu sans jitter.
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Texture;
uniform vec2  u_TexelSize;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec3 rgbCenter = texture(u_Texture, v_UV).rgb;

    vec2 t = u_TexelSize;
    vec3 rgbN  = texture(u_Texture, v_UV + vec2( 0.0, -t.y)).rgb;
    vec3 rgbS  = texture(u_Texture, v_UV + vec2( 0.0,  t.y)).rgb;
    vec3 rgbE  = texture(u_Texture, v_UV + vec2( t.x,  0.0)).rgb;
    vec3 rgbW  = texture(u_Texture, v_UV + vec2(-t.x,  0.0)).rgb;
    vec3 rgbNW = texture(u_Texture, v_UV + vec2(-t.x, -t.y)).rgb;
    vec3 rgbNE = texture(u_Texture, v_UV + vec2( t.x, -t.y)).rgb;
    vec3 rgbSW = texture(u_Texture, v_UV + vec2(-t.x,  t.y)).rgb;
    vec3 rgbSE = texture(u_Texture, v_UV + vec2( t.x,  t.y)).rgb;

    float lC = luma(rgbCenter);
    float lN = luma(rgbN),  lS = luma(rgbS),  lE = luma(rgbE),  lW = luma(rgbW);
    float lNW = luma(rgbNW), lNE = luma(rgbNE), lSW = luma(rgbSW), lSE = luma(rgbSE);

    float lMin = min(lC, min(min(lN, lS), min(lE, lW)));
    float lMax = max(lC, max(max(lN, lS), max(lE, lW)));
    float contrast = lMax - lMin;

    // Zone plate : pas de bord, on sort la couleur brute pour ne pas flouter
    // des détails fins pour rien.
    float threshold = max(0.0312, lMax * 0.125);
    if (contrast < threshold) {
        fragColor = vec4(rgbCenter, 1.0);
        return;
    }

    // Sobel simplifié : direction dominante du gradient (horizontal vs vertical).
    float edgeHoriz = abs(lNW + lN + lNE - lSW - lS - lSE);
    float edgeVert  = abs(lNW + lW + lSW - lNE - lE - lSE);
    bool  horizontal = edgeHoriz >= edgeVert;

    vec3 blurA = horizontal ? rgbN : rgbW;
    vec3 blurB = horizontal ? rgbS : rgbE;
    vec3 blended = (rgbCenter + blurA + blurB) / 3.0;

    // Force le mélange proportionnellement au contraste local : bord net =
    // lissage fort, bord faible = presque inchangé.
    float blendFactor = clamp(contrast * 4.0, 0.0, 1.0);
    fragColor = vec4(mix(rgbCenter, blended, blendFactor), 1.0);
}
