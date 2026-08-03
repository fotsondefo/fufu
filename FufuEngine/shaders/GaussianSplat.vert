#version 430 core

// Each Gaussian is expanded to 6 vertices (two triangles) via gl_VertexID.
// SplatGPU data is read from an SSBO indexed by gl_InstanceID.

struct SplatGPU {
    vec2  screenPos;   // NDC
    float conic_a;
    float conic_b;
    float conic_c;
    float opacity;
    vec3  color;
    float camDepth;
};

layout(std430, binding = 0) readonly buffer SplatBuffer {
    SplatGPU u_Splats[];
};

uniform vec2 u_ScreenSize;  // viewport dimensions in pixels

out vec2  v_UV;       // offset from splat center in pixel coords
out vec3  v_Color;
out float v_Opacity;
out float v_CamDepth; // forwarded to frag for depth test
flat out int  v_ConicA; // packed as floatBitsToInt for precision — actually pass as float
out float v_CA, v_CB, v_CC;

// How many standard deviations the quad covers in the Gaussian's local frame.
// 3σ captures ~99.7% of Gaussian energy.
const float SPLAT_RADIUS_SIGMA = 3.0;

void main()
{
    SplatGPU splat = u_Splats[gl_InstanceID];

    // Decompose 2D covariance (stored as inverse) into an ellipse.
    // The inverse cov2d has the form:
    //   [ a  b ]
    //   [ b  c ]
    // Its eigenvalues give us the semi-axes of the splat ellipse.
    float a = splat.conic_a, b = splat.conic_b, c = splat.conic_c;
    float mid = 0.5 * (a + c);
    float det = a * c - b * b;
    float lambda1 = mid + sqrt(max(0.1, mid * mid - det));
    float lambda2 = mid - sqrt(max(0.1, mid * mid - det));

    // Radii in pixel space (1/sqrt(eigenvalue) gives stddev, * sigma gives extent)
    float r1 = SPLAT_RADIUS_SIGMA / sqrt(max(1e-6, lambda1));
    float r2 = SPLAT_RADIUS_SIGMA / sqrt(max(1e-6, lambda2));

    // Principal axis of the ellipse (eigenvector for lambda1)
    vec2 axis;
    if (abs(b) < 1e-6)
        axis = (a < c) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    else
        axis = normalize(vec2(b, lambda1 - a));
    vec2 perp = vec2(-axis.y, axis.x);

    // Corner offsets (two triangles, 6 vertices)
    const vec2 corners[6] = vec2[6](
        vec2(-1.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0,  1.0),
        vec2(-1.0, -1.0), vec2( 1.0,  1.0), vec2(-1.0,  1.0)
    );
    vec2 c2 = corners[gl_VertexID % 6];

    // Pixel-space offset from splat center
    vec2 pixelOffset = axis * (c2.x * r1) + perp * (c2.y * r2);

    // Convert screen-center from NDC to pixel, add offset, back to NDC
    vec2 centerPx  = (splat.screenPos * 0.5 + 0.5) * u_ScreenSize;
    vec2 posPx     = centerPx + pixelOffset;
    vec2 posNDC    = (posPx / u_ScreenSize) * 2.0 - 1.0;

    gl_Position = vec4(posNDC, 0.0, 1.0);

    v_UV       = pixelOffset;   // pixel-space offset — used for Gaussian eval
    v_Color    = splat.color;
    v_Opacity  = splat.opacity;
    v_CamDepth = splat.camDepth;
    v_CA = a; v_CB = b; v_CC = c;
}
