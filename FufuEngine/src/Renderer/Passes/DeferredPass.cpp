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
                           uint32_t quadVAO,
                           uint32_t skyboxTex,
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

    // Skybox → unité 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyboxTex);

    // Textures matériaux → unités 1..16
    const auto& matTextures = gpu.getMaterialTextures();
    for (int i = 0; i < static_cast<int>(matTextures.size()) && i < 16; ++i)
    {
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, matTextures[i]);
    }

    // G-Buffer → unités 17/18/19
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
