#include "depch.h"
#include "Renderer/Passes/DeferredPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"

namespace Fufu
{

static RHI::Ref<RHI::RHIPipeline> makeGraphicsPipeline(
    RHI::RHIContext& ctx,
    const std::string& vertFile,
    const std::string& fragFile,
    const RHI::GraphicsPipelineDesc& baseDesc)
{
    auto vs = ctx.createShader({ RHI::ShaderStage::Vertex,   loadShaderSource(vertFile), vertFile });
    auto fs = ctx.createShader({ RHI::ShaderStage::Fragment, loadShaderSource(fragFile), fragFile });
    RHI::GraphicsPipelineDesc desc = baseDesc;
    desc.vertexShader   = vs;
    desc.fragmentShader = fs;
    return ctx.createGraphicsPipeline(desc);
}

static void ensureUBO(RHI::RHIContext& ctx, RHI::Ref<RHI::RHIBuffer>& buf,
                      const void* data, size_t size)
{
    if (!buf || buf->getSize() < size)
        buf = ctx.createBuffer({ size, RHI::BufferUsage::Uniform,
                                  RHI::MemoryType::CPU_TO_GPU, data });
    else
        buf->upload(data, size);
}

// ── Init ─────────────────────────────────────────────────────────────────

void DeferredPass::init(RHI::RHIContext& ctx, int width, int height)
{
    m_Ctx = &ctx;

    RHI::GraphicsPipelineDesc desc{};
    desc.depthStencil.depthTestEnable  = false;
    desc.depthStencil.depthWriteEnable = false;

    m_Pipeline = makeGraphicsPipeline(ctx, "FullscreenQuad.vert", "DeferredLighting.frag", desc);

    createAttachments(ctx, width, height);
}

void DeferredPass::shutdown()
{
    m_Pipeline.reset();
    m_OutputTex.reset();
    m_FrameUBO.reset();
    m_ShadowUBO.reset();
}

void DeferredPass::resize(RHI::RHIContext& ctx, int width, int height)
{
    m_OutputTex.reset();
    createAttachments(ctx, width, height);
}

void DeferredPass::createAttachments(RHI::RHIContext& ctx, int w, int h)
{
    RHI::TextureDesc color{};
    color.format = RHI::Format::RGBA32_FLOAT;
    color.width  = static_cast<uint32_t>(w);
    color.height = static_cast<uint32_t>(h);
    color.usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
    m_OutputTex  = ctx.createTexture(color);
}

// ── Render ────────────────────────────────────────────────────────────────

void DeferredPass::render(RHI::RHICommandList& cmd,
                           const GPUScene& gpu,
                           RHI::RHITexture* gPosition,
                           RHI::RHITexture* gNormal,
                           RHI::RHITexture* gUV,
                           const GPUFrameUBO& frame,
                           const GPUShadowUBO& shadow,
                           uint32_t shadowDepthTex,
                           uint32_t ssaoTex,
                           bool     ssaoEnabled,
                           uint32_t quadVAO,
                           uint32_t skyboxTex,
                           uint32_t iblIrradiance,
                           uint32_t iblPrefiltered,
                           uint32_t iblBrdfLut,
                           int width, int height)
{
    RHI::RenderPassDesc pass{};
    pass.colorAttachments.push_back({ m_OutputTex, RHI::LoadOp::Clear, RHI::StoreOp::Store, { 0.f, 0.f, 0.f, 1.f } });
    pass.width  = static_cast<uint32_t>(width);
    pass.height = static_cast<uint32_t>(height);

    cmd.beginLabel("DeferredPass");
    cmd.beginRenderPass(pass);
    cmd.bindPipeline(m_Pipeline.get());

    gpu.bind(cmd);

    // Upload + bind FrameUBO (binding = 0)
    ensureUBO(*m_Ctx, m_FrameUBO, &frame, sizeof(frame));
    cmd.bindUniformBuffer(0, m_FrameUBO.get());

    // Upload + bind ShadowBlock (binding = 2)
    ensureUBO(*m_Ctx, m_ShadowUBO, &shadow, sizeof(shadow));
    cmd.bindUniformBuffer(2, m_ShadowUBO.get());

    // Shadow map → unit 20 (sampler2DShadow)
    glActiveTexture(GL_TEXTURE20);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex);

    // SSAO → unit 21
    glActiveTexture(GL_TEXTURE21);
    glBindTexture(GL_TEXTURE_2D, ssaoTex);

<<<<<<< HEAD
    // IBL textures → units 22 / 23 / 24
    glActiveTexture(GL_TEXTURE22);
    glBindTexture(GL_TEXTURE_2D, iblIrradiance);

    glActiveTexture(GL_TEXTURE23);
    glBindTexture(GL_TEXTURE_2D, iblPrefiltered);

    glActiveTexture(GL_TEXTURE24);
    glBindTexture(GL_TEXTURE_2D, iblBrdfLut);

    // Uniforms outside UBO
=======
    // u_SSAOEnabled: uniform outside UBO
>>>>>>> c984e4df3b1c22d177ab4019fa05d517ae4c3474
    GLint prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glUniform1i(glGetUniformLocation(prog, "u_SSAOEnabled"), ssaoEnabled ? 1 : 0);
    glUniform1i(glGetUniformLocation(prog, "u_IBLEnabled"),
                (iblIrradiance && iblPrefiltered && iblBrdfLut) ? 1 : 0);

    // Skybox → unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyboxTex);

    // Material textures → units 1..16
    const auto& matTextures = gpu.getMaterialTextures();
    for (int i = 0; i < static_cast<int>(matTextures.size()) && i < 16; ++i)
    {
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, matTextures[i]);
    }

    // G-Buffer → units 17/18/19
    cmd.bindTexture(17, gPosition);
    cmd.bindTexture(18, gNormal);
    cmd.bindTexture(19, gUV);

    Profiler::get().beginGPU("DeferredPass");
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    Profiler::get().endGPU("DeferredPass");

    cmd.endRenderPass();
    cmd.endLabel();
}

}
