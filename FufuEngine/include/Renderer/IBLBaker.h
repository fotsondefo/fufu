#pragma once

#include <cstdint>

namespace Fufu
{

// Bakes three IBL textures from an equirectangular HDR environment map.
// All textures are standard GL_TEXTURE_2D (equirectangular) so no cube-map
// complexity is needed in the engine.
//
//  - Irradiance map  : 256×128 RGBA16F — diffuse ambient (cosine-weighted integral)
//  - Prefiltered map : 512×256 RGBA16F, 5 mip levels — specular ambient per roughness
//  - BRDF LUT        : 512×512 RG16F   — split-sum scale/bias table (created once)
//
// Usage:
//   baker.init();                              // once after GL context is ready
//   baker.bake(envTexID, irr, pre);            // call when the skybox changes
//   baker.shutdown();                          // before GL context teardown
class IBLBaker
{
public:
    void init();
    void shutdown();

    // Bakes irradiance and prefiltered env maps from the given equirectangular
    // GL texture. Deletes any previously baked textures stored in the output
    // references before creating the new ones.
    void bake(uint32_t envTexID, uint32_t& outIrradiance, uint32_t& outPrefiltered);

    // BRDF LUT is created once in init() and reused across skybox changes.
    uint32_t getBrdfLut() const { return m_BrdfLut; }

private:
    uint32_t compileProgram(const char* vertSrc, const char* fragFile);
    void     renderToTex(uint32_t fbo, int w, int h, uint32_t prog);

    uint32_t m_QuadVAO = 0;
    uint32_t m_QuadVBO = 0;
    uint32_t m_FBO     = 0;

    uint32_t m_IrradianceProg = 0;
    uint32_t m_PrefilterProg  = 0;
    uint32_t m_BrdfLutProg    = 0;

    uint32_t m_BrdfLut = 0;

    static constexpr int k_IrrW      = 256;
    static constexpr int k_IrrH      = 128;
    static constexpr int k_PreW      = 512;
    static constexpr int k_PreH      = 256;
    static constexpr int k_LutSize   = 512;
    static constexpr int k_MipLevels = 5;
};

} // namespace Fufu
