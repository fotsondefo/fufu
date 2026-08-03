#pragma once
#include <glm/glm.hpp>

namespace Fufu
{

// Matches exactly the std140 layout of FrameBlock (binding = 0).
// Adjacent vec3 + float avoids implicit std140 padding
// (vec3 → align 16, a float after fills the 4 free bytes).
struct GPUFrameUBO
{
    glm::mat4 viewProj;         // 0-63
    glm::vec3 camPos;           // 64-75
    float     _p0 = 0.f;       // 76-79
    glm::vec3 camForward;       // 80-91
    float     camFov = 0.f;    // 92-95
    glm::vec3 camRight;         // 96-107
    float     camAspect = 0.f; // 108-111
    glm::vec3 camUp;            // 112-123
    float     exposure = 0.f;  // 124-127
    int       lightCount = 0;  // 128-131
    int       hasSkybox = 0;   // 132-135
    float     skyboxIntensity = 0.f; // 136-139
    float     _p1 = 0.f;      // 140-143
};                              // total : 144 bytes
static_assert(sizeof(GPUFrameUBO) == 144, "GPUFrameUBO std140 size mismatch");

// Matches the std140 layout of DrawBlock (binding = 1).
struct GPUDrawUBO
{
    glm::mat4 transform;        // 0-63
    glm::mat4 invTransform;     // 64-127
    int       triOffset = 0;   // 128-131
    int       materialIndex = 0; // 132-135
    int       _pad[2] = {};    // 136-143
};                              // total : 144 bytes
static_assert(sizeof(GPUDrawUBO) == 144, "GPUDrawUBO std140 size mismatch");

// Matches the std140 layout of ShadowBlock (UBO binding = 2).
// Passed to DeferredLighting.frag for PCF shadow mapping.
struct GPUShadowUBO
{
    glm::mat4 lightSpaceMatrix;    // 0-63
    float     shadowBias    = 0.005f; // 64-67
    int       shadowEnabled = 0;      // 68-71
    float     _pad[2]       = {};     // 72-79
};                                    // total : 80 bytes
static_assert(sizeof(GPUShadowUBO) == 80, "GPUShadowUBO std140 size mismatch");

} // namespace Fufu
