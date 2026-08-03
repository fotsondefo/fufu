#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <array>

namespace Fufu
{

class SSAOPass
{
public:
    void init  (int width, int height);
    void shutdown();
    void resize(int width, int height);

    // Executes SSAO + blur; returns the blurred R8 texture (AO factor).
    uint32_t execute(uint32_t gPosTex,
                     uint32_t gNrmTex,
                     uint32_t quadVAO,
                     int width, int height,
                     const glm::mat4& view,
                     const glm::mat4& proj,
                     float radius,
                     float bias,
                     float strength,
                     int   numSamples);

    uint32_t getAOTexture()     const { return m_BlurTex; }
    uint32_t getRawAOTexture()  const { return m_AOTex; }

private:
    void createTextures(int w, int h);
    void generateKernel();
    void generateNoise();

    uint32_t m_AOProgram   = 0;
    uint32_t m_BlurProgram = 0;

    uint32_t m_AOTex   = 0;   // R8, full resolution  (raw)
    uint32_t m_BlurTex = 0;   // R8, full resolution  (smoothed)
    uint32_t m_NoiseTex = 0;  // RG16F, 4x4

    uint32_t m_AOFBO   = 0;
    uint32_t m_BlurFBO = 0;

    std::array<glm::vec3, 64> m_Kernel;
};

} // namespace Fufu
