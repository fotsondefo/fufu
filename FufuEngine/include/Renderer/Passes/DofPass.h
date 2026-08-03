#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Fufu
{

// Depth of Field in two passes (deferred only):
//   1. DofCoC  — normalized Circle of Confusion [0,1] → R8
//   2. DofBlur — Vogel disk weighted by CoC            → RGBA32F (HDR)
class DofPass
{
public:
    void init  (int width, int height);
    void shutdown();
    void resize(int width, int height);

    // Executes both passes and returns the blurred HDR texture.
    uint32_t execute(uint32_t hdrTex,
                     uint32_t gPosTex,
                     uint32_t quadVAO,
                     int width, int height,
                     const glm::vec3& camPos,
                     float focusDist,
                     float focusRange,
                     float maxBlurRadius,
                     int   samples);

    uint32_t getOutputTexture() const { return m_BlurTex; }

private:
    void createTextures(int w, int h);

    uint32_t m_CoCProgram  = 0;
    uint32_t m_BlurProgram = 0;

    uint32_t m_CoCTex  = 0;   // R8,       full resolution
    uint32_t m_BlurTex = 0;   // RGBA32F,  full resolution

    uint32_t m_CoCFBO  = 0;
    uint32_t m_BlurFBO = 0;
};

} // namespace Fufu
