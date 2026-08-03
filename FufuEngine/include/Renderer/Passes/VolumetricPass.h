#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Fufu
{

// Volumetric light scattering in two passes:
//   1. Raymarch at half resolution → RGBA16F (inscatter.rgb, transmittance)
//   2. Additive composite at full resolution → RGBA32F HDR
class VolumetricPass
{
public:
    void init  (int width, int height);
    void shutdown();
    void resize(int width, int height);

    // Returns the composited HDR texture.
    uint32_t execute(uint32_t hdrTex,
                     uint32_t gPosTex,
                     uint32_t shadowDepthTex,
                     uint32_t quadVAO,
                     int width, int height,
                     // Camera
                     const glm::vec3& camPos,
                     const glm::vec3& camForward,
                     const glm::vec3& camRight,
                     const glm::vec3& camUp,
                     float camFov, float camAspect,
                     // Shadow / light
                     const glm::mat4& lightSpaceMatrix,
                     float shadowBias, bool shadowEnabled,
                     const glm::vec3& lightDir,
                     const glm::vec3& lightColor,
                     float lightIntensity,
                     // Volume
                     int steps, float density, float scattering,
                     float anisotropy, float ambient, float maxDist);

    uint32_t getOutputTexture() const { return m_CompositeTex; }

private:
    void createTextures(int w, int h);

    uint32_t m_MarchProgram     = 0;
    uint32_t m_CompositeProgram = 0;

    uint32_t m_ScatterTex   = 0;   // RGBA16F, half resolution
    uint32_t m_CompositeTex = 0;   // RGBA32F, full resolution

    uint32_t m_ScatterFBO   = 0;
    uint32_t m_CompositeFBO = 0;
};

} // namespace Fufu
