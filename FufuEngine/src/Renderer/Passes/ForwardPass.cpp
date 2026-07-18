#include "depch.h"
#include "Renderer/Passes/ForwardPass.h"
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
    auto vs = ctx.createShader({ RHI::ShaderStage::Vertex,   loadShaderSource(vertFile) });
    auto fs = ctx.createShader({ RHI::ShaderStage::Fragment, loadShaderSource(fragFile) });
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

void ForwardPass::init(RHI::RHIContext& ctx, int width, int height)
{
    m_Ctx = &ctx;

    // Programme ciel : pas de depth test ni d'écriture depth
    {
        RHI::GraphicsPipelineDesc desc{};
        desc.depthStencil.depthTestEnable  = false;
        desc.depthStencil.depthWriteEnable = false;
        m_SkyPipeline = makeGraphicsPipeline(ctx, "FullscreenQuad.vert", "Sky.frag", desc);
    }

    // Programme géométrie PBR
    {
        RHI::GraphicsPipelineDesc desc{};
        desc.depthStencil = { true, true, RHI::CompareOp::Less };
        m_GeoPipeline = makeGraphicsPipeline(ctx, "GBuffer.vert", "Forward.frag", desc);
    }

    createAttachments(ctx, width, height);
}

void ForwardPass::shutdown()
{
    m_SkyPipeline.reset();
    m_GeoPipeline.reset();
    m_OutputTex.reset();
    m_DepthTex.reset();
    m_FrameUBO.reset();
    m_DrawUBO.reset();
}

void ForwardPass::resize(RHI::RHIContext& ctx, int width, int height)
{
    m_OutputTex.reset();
    m_DepthTex.reset();
    createAttachments(ctx, width, height);
}

void ForwardPass::createAttachments(RHI::RHIContext& ctx, int w, int h)
{
    RHI::TextureDesc color{};
    color.format = RHI::Format::RGBA32_FLOAT;
    color.width  = static_cast<uint32_t>(w);
    color.height = static_cast<uint32_t>(h);
    color.usage  = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
    m_OutputTex  = ctx.createTexture(color);

    RHI::TextureDesc depth{};
    depth.format = RHI::Format::DEPTH24_STENCIL8;
    depth.width  = static_cast<uint32_t>(w);
    depth.height = static_cast<uint32_t>(h);
    depth.usage  = RHI::TextureUsage::DepthAttachment;
    m_DepthTex   = ctx.createTexture(depth);
}

// ── Render ────────────────────────────────────────────────────────────────

void ForwardPass::render(RHI::RHICommandList& cmd,
                          const GPUScene& gpu,
                          const GPUFrameUBO& frame,
                          uint32_t quadVAO,
                          uint32_t skyboxTex,
                          int width, int height)
{
    RHI::RenderPassDesc pass{};
    pass.colorAttachments.push_back({ m_OutputTex, RHI::LoadOp::Clear, RHI::StoreOp::Store, { 0.f, 0.f, 0.f, 1.f } });
    pass.depthAttachment = RHI::DepthAttachment{ m_DepthTex, RHI::LoadOp::Clear, RHI::StoreOp::DontCare, 1.0f };
    pass.width  = static_cast<uint32_t>(width);
    pass.height = static_cast<uint32_t>(height);

    // Upload FrameUBO une fois pour les deux sous-passes
    ensureUBO(*m_Ctx, m_FrameUBO, &frame, sizeof(frame));

    cmd.beginLabel("ForwardPass");
    cmd.beginRenderPass(pass);

    // ── Passe ciel ────────────────────────────────────────────────────────
    cmd.bindPipeline(m_SkyPipeline.get()); // depth test OFF, depth write OFF
    cmd.bindUniformBuffer(0, m_FrameUBO.get());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skyboxTex);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ── Passe géométrie PBR ───────────────────────────────────────────────
    const auto& instances = gpu.getInstances();
    const auto& triCounts = gpu.getInstanceTriCounts();
    if (!instances.empty())
    {
        cmd.bindPipeline(m_GeoPipeline.get()); // depth test ON, depth write ON
        gpu.bind(cmd);
        cmd.bindUniformBuffer(0, m_FrameUBO.get());

        const auto& matTextures = gpu.getMaterialTextures();
        for (int i = 0; i < static_cast<int>(matTextures.size()) && i < 16; ++i)
        {
            glActiveTexture(GL_TEXTURE1 + i);
            glBindTexture(GL_TEXTURE_2D, matTextures[i]);
        }

        Profiler::get().beginGPU("ForwardPass");
        for (std::size_t i = 0; i < instances.size(); ++i)
        {
            const GPUInstance& inst = instances[i];
            int triCount = triCounts[i];
            if (triCount <= 0) continue;

            GPUDrawUBO draw;
            draw.transform     = inst.transform;
            draw.invTransform  = inst.invTransform;
            draw.triOffset     = inst.blasTriOffset;
            draw.materialIndex = inst.materialIndex;
            ensureUBO(*m_Ctx, m_DrawUBO, &draw, sizeof(draw));
            cmd.bindUniformBuffer(1, m_DrawUBO.get());

            cmd.draw(static_cast<uint32_t>(triCount * 3));
        }
        Profiler::get().endGPU("ForwardPass");
    }

    cmd.endRenderPass();
    cmd.endLabel();
}

}
