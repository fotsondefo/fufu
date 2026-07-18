#pragma once
#include <glm/glm.hpp>

namespace Fufu
{

// Correspond exactement au layout std140 de FrameBlock (binding = 0).
// Les vec3 + float voisin évitent le padding implicite std140
// (vec3 → align 16, un float après comble les 4 octets libres).
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

// Correspond au layout std140 de DrawBlock (binding = 1).
struct GPUDrawUBO
{
    glm::mat4 transform;        // 0-63
    glm::mat4 invTransform;     // 64-127
    int       triOffset = 0;   // 128-131
    int       materialIndex = 0; // 132-135
    int       _pad[2] = {};    // 136-143
};                              // total : 144 bytes
static_assert(sizeof(GPUDrawUBO) == 144, "GPUDrawUBO std140 size mismatch");

} // namespace Fufu
