#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>

namespace Fufu
{

class Scene;

// Heterogeneous volume rendering: up to 4 simultaneous AABB volumes.
// Single-pass fullscreen raymarching with Beer-Lambert transmittance,
// Henyey-Greenstein phase, and optional procedural 3D noise density field.
class VolumePass
{
public:
    void init    (int width, int height);
    void shutdown();
    void resize  (int width, int height);

    // Returns the composited HDR texture (or hdrTex unchanged if no volumes).
    uint32_t execute(Scene&    scene,
                     uint32_t hdrTex,
                     uint32_t gPosTex,
                     uint32_t shadowDepthTex,
                     uint32_t quadVAO,
                     int width, int height,
                     const glm::vec3& camPos,
                     const glm::vec3& camForward,
                     const glm::vec3& camRight,
                     const glm::vec3& camUp,
                     float camFov, float camAspect,
                     const glm::mat4& lightSpaceMatrix,
                     float shadowBias, bool shadowEnabled,
                     const glm::vec3& lightDir,
                     const glm::vec3& lightColor,
                     float lightIntensity);

    uint32_t getOutputTexture() const { return m_OutputTex; }

private:
    uint32_t getOrCreateNoiseTex(uint64_t hash,
                                  int   noiseType,
                                  float scale,
                                  int   octaves,
                                  float lacunarity,
                                  float gain);

    void createTextures(int w, int h);
    void deleteTextures();

    uint32_t m_Program    = 0;
    uint32_t m_OutputTex  = 0;   // RGBA32F full-resolution
    uint32_t m_FBO        = 0;
    uint32_t m_UBO        = 0;   // VolumeBlock (binding = 5)
    uint32_t m_DefaultTex = 0;   // 1×1×1 white — used when noiseType == None

    // Noise texture cache: param hash → GL_TEXTURE_3D
    std::unordered_map<uint64_t, uint32_t> m_NoiseTex;
};

} // namespace Fufu
