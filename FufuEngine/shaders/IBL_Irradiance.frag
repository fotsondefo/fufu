#version 430 core

// Convolves an equirectangular environment map over the hemisphere to produce
// a diffuse irradiance map (also equirectangular).
// Each output texel encodes the total incoming radiance from the upper
// hemisphere for the surface normal that texel represents.

in  vec2 v_UV;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D u_Env;

const float PI = 3.14159265359;

// Equirectangular UV from a world-space direction.
vec2 equirectUV(vec3 dir) {
    return vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5,
                acos(clamp(dir.y, -1.0, 1.0)) / PI);
}

// Decode a texel UV to a world-space direction (inverse of equirectUV).
vec3 uvToDir(vec2 uv) {
    float phi   = (uv.x - 0.5) * 2.0 * PI; // [-PI, PI]
    float theta = uv.y * PI;                 // [  0,  PI]
    return vec3(sin(theta) * cos(phi),
                cos(theta),
                sin(theta) * sin(phi));
}

void main() {
    vec3 N = normalize(uvToDir(v_UV));

    // Build an orthonormal basis around N for the hemisphere samples.
    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);

    vec3  irradiance = vec3(0.0);
    float nSamples   = 0.0;

    // Riemann sum over the upper hemisphere.
    // dPhi / dTheta trade off quality vs bake time. 0.05 rad ≈ 2.9° gives
    // good results while keeping the bake under a second on modern hardware.
    const float dPhi   = 0.05;
    const float dTheta = 0.05;

    for (float phi = 0.0; phi < 2.0 * PI; phi += dPhi) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += dTheta) {
            // Convert spherical (tangent space) to world-space direction.
            vec3 wi = sin(theta) * cos(phi) * right
                    + sin(theta) * sin(phi) * up
                    + cos(theta)            * N;

            irradiance += texture(u_Env, equirectUV(wi)).rgb
                        * cos(theta)   // Lambert cosine term
                        * sin(theta);  // spherical area element
            nSamples   += 1.0;
        }
    }

    fragColor = vec4(PI * irradiance / max(nSamples, 1.0), 1.0);
}
