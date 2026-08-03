#include "depch.h"
#include "Renderer/IBLBaker.h"
#include "Renderer/ShaderUtils.h"

namespace Fufu
{

// Minimal fullscreen-quad vertex shader embedded as a string so the baker
// has no file dependency of its own (FullscreenQuad.vert belongs to the
// frame-level RHI pipeline).
static const char* k_QuadVert = R"GLSL(
#version 430 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() { gl_Position = vec4(a_Position, 0.0, 1.0); v_UV = a_UV; }
)GLSL";

// ── Helpers ──────────────────────────────────────────────────────────────────

static uint32_t compileShader(GLenum type, const char* src)
{
    uint32_t id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    int ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        FUFU_ERROR("IBLBaker shader compile error: {}", log);
    }
    return id;
}

uint32_t IBLBaker::compileProgram(const char* vertSrc, const char* fragFile)
{
    std::string fragSrc = loadShaderSource(fragFile);
    if (fragSrc.empty()) {
        FUFU_ERROR("IBLBaker: could not load shader '{}'", fragFile);
        return 0;
    }

    uint32_t vs   = compileShader(GL_VERTEX_SHADER,   vertSrc);
    uint32_t fs   = compileShader(GL_FRAGMENT_SHADER, fragSrc.c_str());
    uint32_t prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        FUFU_ERROR("IBLBaker program link error: {}", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void IBLBaker::renderToTex(uint32_t fbo, int w, int h, uint32_t prog)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glUseProgram(prog);
    glBindVertexArray(m_QuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ── Init / Shutdown ───────────────────────────────────────────────────────────

void IBLBaker::init()
{
    // Fullscreen quad VAO (same layout as the engine's shared quad)
    const float quad[] = {
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
        -1.f, -1.f,  0.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
        -1.f,  1.f,  0.f, 1.f,
    };
    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    glGenFramebuffers(1, &m_FBO);

    // Compile baking programs
    m_IrradianceProg = compileProgram(k_QuadVert, "IBL_Irradiance.frag");
    m_PrefilterProg  = compileProgram(k_QuadVert, "IBL_Prefilter.frag");
    m_BrdfLutProg    = compileProgram(k_QuadVert, "IBL_BrdfLut.frag");

    // BRDF LUT: computed once, independent of the env map
    glGenTextures(1, &m_BrdfLut);
    glBindTexture(GL_TEXTURE_2D, m_BrdfLut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, k_LutSize, k_LutSize,
                 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_BrdfLut, 0);
    renderToTex(m_FBO, k_LutSize, k_LutSize, m_BrdfLutProg);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    FUFU_INFO("IBLBaker: BRDF LUT ready ({}×{})", k_LutSize, k_LutSize);
}

void IBLBaker::shutdown()
{
    if (m_BrdfLut)        { glDeleteTextures(1, &m_BrdfLut);      m_BrdfLut        = 0; }
    if (m_FBO)            { glDeleteFramebuffers(1, &m_FBO);       m_FBO            = 0; }
    if (m_QuadVAO)        { glDeleteVertexArrays(1, &m_QuadVAO);   m_QuadVAO        = 0; }
    if (m_QuadVBO)        { glDeleteBuffers(1, &m_QuadVBO);        m_QuadVBO        = 0; }
    if (m_IrradianceProg) { glDeleteProgram(m_IrradianceProg);     m_IrradianceProg = 0; }
    if (m_PrefilterProg)  { glDeleteProgram(m_PrefilterProg);      m_PrefilterProg  = 0; }
    if (m_BrdfLutProg)    { glDeleteProgram(m_BrdfLutProg);        m_BrdfLutProg    = 0; }
}

// ── Bake ─────────────────────────────────────────────────────────────────────

void IBLBaker::bake(uint32_t envTexID,
                    uint32_t& outIrradiance,
                    uint32_t& outPrefiltered)
{
    if (!m_IrradianceProg || !m_PrefilterProg) {
        FUFU_ERROR("IBLBaker::bake called before init()");
        return;
    }

    // Free previous textures
    if (outIrradiance)  { glDeleteTextures(1, &outIrradiance);  outIrradiance  = 0; }
    if (outPrefiltered) { glDeleteTextures(1, &outPrefiltered); outPrefiltered = 0; }

    // Bind the env map to unit 0 (both shaders use layout(binding=0))
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, envTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // ── 1. Irradiance map ────────────────────────────────────────────────────
    glGenTextures(1, &outIrradiance);
    glBindTexture(GL_TEXTURE_2D, outIrradiance);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 k_IrrW, k_IrrH, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, outIrradiance, 0);
    renderToTex(m_FBO, k_IrrW, k_IrrH, m_IrradianceProg);

    FUFU_INFO("IBLBaker: irradiance map ready ({}×{})", k_IrrW, k_IrrH);

    // ── 2. Pre-filtered env map (one pass per mip level) ─────────────────────
    glGenTextures(1, &outPrefiltered);
    glBindTexture(GL_TEXTURE_2D, outPrefiltered);

    // Allocate all mip levels upfront
    for (int mip = 0; mip < k_MipLevels; ++mip) {
        int mw = k_PreW >> mip;
        int mh = k_PreH >> mip;
        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA16F,
                     mw, mh, 0, GL_RGBA, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, k_MipLevels - 1);

    int roughnessLoc = glGetUniformLocation(m_PrefilterProg, "u_Roughness");

    for (int mip = 0; mip < k_MipLevels; ++mip) {
        int   mw        = k_PreW >> mip;
        int   mh        = k_PreH >> mip;
        float roughness = static_cast<float>(mip) / static_cast<float>(k_MipLevels - 1);

        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, outPrefiltered, mip);

        glViewport(0, 0, mw, mh);
        glUseProgram(m_PrefilterProg);
        glUniform1f(roughnessLoc, roughness);
        glBindVertexArray(m_QuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    FUFU_INFO("IBLBaker: prefiltered env map ready ({}×{}, {} mips)",
              k_PreW, k_PreH, k_MipLevels);
}

} // namespace Fufu
