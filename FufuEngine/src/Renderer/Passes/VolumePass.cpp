#include "depch.h"
#include "Renderer/Passes/VolumePass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"
#include "Project/Scene/Scene.h"
#include "Project/Components.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace Fufu
{

// ── GPU layout matching Volume.frag UBO (std140, binding = 5) ───────────────

struct GPUVolumeData
{
    glm::vec4 boundsMin;  // xyz = worldMin,  w = density
    glm::vec4 boundsMax;  // xyz = worldMax,  w = scattering
    glm::vec4 albedo;     // xyz = albedo,    w = absorption
    glm::vec4 emission;   // xyz = emColor,   w = emissionStrength
    glm::vec4 params;     // x = anisotropy,  y = marchSteps (as float)
};

struct GPUVolumeBlock
{
    GPUVolumeData volumes[4];  // 4 × 80 = 320 bytes
    int volumeCount;           // +4
    int _pad[3];               // +12 → 336 bytes total
};
static_assert(sizeof(GPUVolumeBlock) == 336, "GPUVolumeBlock std140 mismatch");

// ── Noise generation ─────────────────────────────────────────────────────────

static float hashFloat(int x, int y, int z)
{
    uint32_t h = static_cast<uint32_t>(x * 1087u + y * 2311u + z * 4513u + 12345u);
    h = (h ^ (h >> 16)) * 0x45d9f3bu;
    h = (h ^ (h >> 16)) * 0x45d9f3bu;
    h ^= (h >> 16);
    return static_cast<float>(h & 0xFFFFu) / 65536.f;
}

static float smoothstep3(float x)
{
    return x * x * (3.f - 2.f * x);
}

static float valueNoise3D(float x, float y, float z)
{
    int   ix = static_cast<int>(std::floor(x));
    int   iy = static_cast<int>(std::floor(y));
    int   iz = static_cast<int>(std::floor(z));
    float fx = smoothstep3(x - ix);
    float fy = smoothstep3(y - iy);
    float fz = smoothstep3(z - iz);

    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };

    float v000 = hashFloat(ix,   iy,   iz  );
    float v100 = hashFloat(ix+1, iy,   iz  );
    float v010 = hashFloat(ix,   iy+1, iz  );
    float v110 = hashFloat(ix+1, iy+1, iz  );
    float v001 = hashFloat(ix,   iy,   iz+1);
    float v101 = hashFloat(ix+1, iy,   iz+1);
    float v011 = hashFloat(ix,   iy+1, iz+1);
    float v111 = hashFloat(ix+1, iy+1, iz+1);

    return lerp(
        lerp(lerp(v000, v100, fx), lerp(v010, v110, fx), fy),
        lerp(lerp(v001, v101, fx), lerp(v011, v111, fx), fy),
        fz);
}

// Generates a flat float array [res^3] of density values in [0, 1].
static std::vector<float> generateNoiseTex3D(int res, int noiseType,
                                              float scale, int octaves,
                                              float lacunarity, float gain)
{
    std::vector<float> data(static_cast<size_t>(res) * res * res);
    for (int z = 0; z < res; ++z)
    for (int y = 0; y < res; ++y)
    for (int x = 0; x < res; ++x)
    {
        float fx = (x + 0.5f) / res * scale;
        float fy = (y + 0.5f) / res * scale;
        float fz = (z + 0.5f) / res * scale;

        float val = 0.f;
        if (noiseType == 1) // Value
        {
            val = valueNoise3D(fx, fy, fz);
        }
        else // FBM (noiseType == 2)
        {
            float amp = 0.5f, freq = 1.f, total = 0.f, maxAmp = 0.f;
            for (int o = 0; o < octaves; ++o)
            {
                total  += amp * valueNoise3D(fx * freq, fy * freq, fz * freq);
                maxAmp += amp;
                amp    *= gain;
                freq   *= lacunarity;
            }
            val = (maxAmp > 0.f) ? total / maxAmp : 0.f;
        }
        data[static_cast<size_t>(z) * res * res + y * res + x] = val;
    }
    return data;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static uint32_t buildProg(const char* frag)
{
    uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("FullscreenQuad.vert"));
    uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource(frag));
    uint32_t p  = linkProgram({ vs, fs });
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static uint32_t makeRGBA32F(int w, int h)
{
    uint32_t id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

static uint32_t makeFBO(uint32_t tex)
{
    uint32_t fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        FUFU_ERROR("VolumePass: FBO incomplet");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

// ── Init / Shutdown ───────────────────────────────────────────────────────────

void VolumePass::init(int width, int height)
{
    m_Program = buildProg("Volume.frag");

    // UBO — VolumeBlock at binding 5
    glGenBuffers(1, &m_UBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_UBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUVolumeBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, m_UBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Default 1×1×1 white density texture (noiseType == None)
    float one = 1.f;
    glGenTextures(1, &m_DefaultTex);
    glBindTexture(GL_TEXTURE_3D, m_DefaultTex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, 1, 1, 1, 0, GL_RED, GL_FLOAT, &one);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_3D, 0);

    createTextures(width, height);
}

void VolumePass::shutdown()
{
    glDeleteProgram(m_Program);
    glDeleteBuffers(1, &m_UBO);
    glDeleteTextures(1, &m_DefaultTex);
    deleteTextures();

    for (auto& [h, tex] : m_NoiseTex)
        glDeleteTextures(1, &tex);
    m_NoiseTex.clear();

    m_Program = m_UBO = m_DefaultTex = 0;
}

void VolumePass::resize(int width, int height)
{
    deleteTextures();
    createTextures(width, height);
}

void VolumePass::createTextures(int w, int h)
{
    m_OutputTex = makeRGBA32F(w, h);
    m_FBO       = makeFBO(m_OutputTex);
}

void VolumePass::deleteTextures()
{
    glDeleteTextures(1, &m_OutputTex);
    glDeleteFramebuffers(1, &m_FBO);
    m_OutputTex = m_FBO = 0;
}

// ── Noise texture cache ───────────────────────────────────────────────────────

uint32_t VolumePass::getOrCreateNoiseTex(uint64_t hash, int noiseType,
                                          float scale, int octaves,
                                          float lacunarity, float gain)
{
    auto it = m_NoiseTex.find(hash);
    if (it != m_NoiseTex.end()) return it->second;

    const int res = 64; // 64×64×64 = 1 MB per texture
    auto data = generateNoiseTex3D(res, noiseType, scale, octaves, lacunarity, gain);

    uint32_t id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_3D, id);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, res, res, res, 0, GL_RED, GL_FLOAT, data.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_3D, 0);

    m_NoiseTex[hash] = id;
    return id;
}

// ── Execute ───────────────────────────────────────────────────────────────────

uint32_t VolumePass::execute(Scene&    scene,
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
                              float lightIntensity)
{
    // Gather up to 4 volumes from the scene
    GPUVolumeBlock block = {};
    uint32_t densityTextures[4] = { m_DefaultTex, m_DefaultTex, m_DefaultTex, m_DefaultTex };

    auto& reg  = scene.getRegistry();
    auto  view = reg.view<TransformComponent, VolumeComponent>();

    for (auto entity : view)
    {
        if (block.volumeCount >= 4) break;

        auto& t = view.get<TransformComponent>(entity);
        auto& v = view.get<VolumeComponent>(entity);

        glm::vec3 center  = t.position;
        glm::vec3 halfExt = t.scale * 0.5f;

        int idx = block.volumeCount;
        GPUVolumeData& gv = block.volumes[idx];
        gv.boundsMin = glm::vec4(center - halfExt, v.density);
        gv.boundsMax = glm::vec4(center + halfExt, v.scattering);
        gv.albedo    = glm::vec4(v.albedo,   v.absorption);
        gv.emission  = glm::vec4(v.emission, v.emissionStrength);
        gv.params    = glm::vec4(v.anisotropy, static_cast<float>(v.marchSteps), 0.f, 0.f);

        if (v.noiseType != VolumeNoiseType::None)
        {
            // Build a stable hash from the noise parameters
            auto floatBits = [](float f) -> uint64_t {
                uint32_t bits;
                std::memcpy(&bits, &f, sizeof(bits));
                return static_cast<uint64_t>(bits);
            };
            uint64_t h = 0;
            auto mix = [&](uint64_t x) {
                h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            };
            mix(static_cast<uint64_t>(v.noiseType));
            mix(floatBits(v.noiseScale));
            mix(static_cast<uint64_t>(v.noiseOctaves));
            mix(floatBits(v.noiseLacunarity));
            mix(floatBits(v.noiseGain));

            densityTextures[idx] = getOrCreateNoiseTex(h,
                static_cast<int>(v.noiseType),
                v.noiseScale, v.noiseOctaves,
                v.noiseLacunarity, v.noiseGain);
        }

        block.volumeCount++;
    }

    // No volumes: return the HDR texture unchanged
    if (block.volumeCount == 0) return hdrTex;

    // Upload UBO
    glBindBuffer(GL_UNIFORM_BUFFER, m_UBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GPUVolumeBlock), &block);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Render
    Profiler::get().beginGPU("VolumePass");

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(quadVAO);

    glUseProgram(m_Program);
    auto loc = [&](const char* n) { return glGetUniformLocation(m_Program, n); };

    // Bind HDR + GBuffer + shadow
    glUniform1i(loc("u_HdrTex"),   0);
    glUniform1i(loc("u_GPosTex"),  1);
    glUniform1i(loc("u_ShadowTex"), 2);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdrTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gPosTex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, shadowDepthTex);

    // Bind density textures
    const char* densNames[] = { "u_DensityTex0", "u_DensityTex1",
                                 "u_DensityTex2", "u_DensityTex3" };
    for (int i = 0; i < 4; ++i)
    {
        glUniform1i(loc(densNames[i]), 3 + i);
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE3 + i));
        glBindTexture(GL_TEXTURE_3D, densityTextures[i]);
    }

    // Camera uniforms
    glUniform3fv(loc("u_CamPos"),     1, glm::value_ptr(camPos));
    glUniform3fv(loc("u_CamForward"), 1, glm::value_ptr(camForward));
    glUniform3fv(loc("u_CamRight"),   1, glm::value_ptr(camRight));
    glUniform3fv(loc("u_CamUp"),      1, glm::value_ptr(camUp));
    glUniform1f (loc("u_CamFov"),     camFov);
    glUniform1f (loc("u_CamAspect"),  camAspect);

    // Shadow uniforms
    glUniformMatrix4fv(loc("u_LightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glUniform1f(loc("u_ShadowBias"),    shadowBias);
    glUniform1i(loc("u_ShadowEnabled"), shadowEnabled ? 1 : 0);

    // Light uniforms
    glUniform3fv(loc("u_LightDir"),      1, glm::value_ptr(lightDir));
    glUniform3fv(loc("u_LightColor"),    1, glm::value_ptr(lightColor));
    glUniform1f (loc("u_LightIntensity"), lightIntensity);

    // UBO binding (already bound to slot 5 at init; re-bind in case context was reset)
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, m_UBO);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    Profiler::get().endGPU("VolumePass");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);

    // Reset active texture to 0
    glActiveTexture(GL_TEXTURE0);

    return m_OutputTex;
}

} // namespace Fufu
