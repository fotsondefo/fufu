#include "depch.h"
#include "Renderer/Passes/GBufferPass.h"
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

void GBufferPass::init(RHI::RHIContext& ctx, int width, int height)
{
    m_Ctx = &ctx;

    RHI::GraphicsPipelineDesc desc{};
    desc.depthStencil = { true, true, RHI::CompareOp::Less };
    desc.rasterizer   = {};

    m_Pipeline = makeGraphicsPipeline(ctx, "GBuffer.vert", "GBuffer.frag", desc);

    createAttachments(ctx, width, height);
}

void GBufferPass::shutdown()
{
    m_Pipeline.reset();
    m_PositionTex.reset();
    m_NormalTex.reset();
    m_UVTex.reset();
    m_DepthTex.reset();
    m_FrameUBO.reset();
    m_DrawUBO.reset();
}

void GBufferPass::resize(RHI::RHIContext& ctx, int width, int height)
{
    m_PositionTex.reset();
    m_NormalTex.reset();
    m_UVTex.reset();
    m_DepthTex.reset();
    createAttachments(ctx, width, height);
}

void GBufferPass::createAttachments(RHI::RHIContext& ctx, int w, int h)
{
    RHI::TextureDesc rgba32f{};
    rgba32f.format    = RHI::Format::RGBA32_FLOAT;
    rgba32f.width     = static_cast<uint32_t>(w);
    rgba32f.height    = static_cast<uint32_t>(h);
    rgba32f.usage     = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;

    m_PositionTex = ctx.createTexture(rgba32f);
    m_NormalTex   = ctx.createTexture(rgba32f);
    m_UVTex       = ctx.createTexture(rgba32f);

    RHI::TextureDesc depth{};
    depth.format = RHI::Format::DEPTH24_STENCIL8;
    depth.width  = static_cast<uint32_t>(w);
    depth.height = static_cast<uint32_t>(h);
    depth.usage  = RHI::TextureUsage::DepthAttachment;

    m_DepthTex = ctx.createTexture(depth);
}

// ── Render ────────────────────────────────────────────────────────────────

void GBufferPass::render(RHI::RHICommandList& cmd,
                          const GPUScene& gpu,
                          const GPUFrameUBO& frame,
                          int width, int height)
{
    const auto& instances = gpu.getInstances();
    const auto& triCounts = gpu.getInstanceTriCounts();
    if (instances.empty()) return;

    RHI::RenderPassDesc pass{};
    // position : w=0 → ciel (clear value 0)
    pass.colorAttachments.push_back({ m_PositionTex, RHI::LoadOp::Clear, RHI::StoreOp::Store, { 0.f, 0.f, 0.f, 0.f } });
    pass.colorAttachments.push_back({ m_NormalTex,   RHI::LoadOp::Clear, RHI::StoreOp::Store, { 0.f, 0.f, 0.f, 0.f } });
    pass.colorAttachments.push_back({ m_UVTex,       RHI::LoadOp::Clear, RHI::StoreOp::Store, { 0.f, 0.f, 0.f, 0.f } });
    pass.depthAttachment = RHI::DepthAttachment{ m_DepthTex, RHI::LoadOp::Clear, RHI::StoreOp::DontCare, 1.0f };
    pass.width  = static_cast<uint32_t>(width);
    pass.height = static_cast<uint32_t>(height);

    cmd.beginLabel("GBufferPass");
    cmd.beginRenderPass(pass);
    cmd.bindPipeline(m_Pipeline.get());

    gpu.bind(cmd);

    // Upload + bind FrameUBO (binding = 0)
    ensureUBO(*m_Ctx, m_FrameUBO, &frame, sizeof(frame));
    cmd.bindUniformBuffer(0, m_FrameUBO.get());

    Profiler::get().beginGPU("GBufferPass");
    for (std::size_t i = 0; i < instances.size(); ++i)
    {
        const GPUInstance& inst = instances[i];
        int triCount = triCounts[i];
        if (triCount <= 0) continue;

        // Upload + bind DrawUBO (binding = 1)
        GPUDrawUBO draw;
        draw.transform     = inst.transform;
        draw.invTransform  = inst.invTransform;
        draw.triOffset     = inst.blasTriOffset;
        draw.materialIndex = inst.materialIndex;
        ensureUBO(*m_Ctx, m_DrawUBO, &draw, sizeof(draw));
        cmd.bindUniformBuffer(1, m_DrawUBO.get());

        cmd.draw(static_cast<uint32_t>(triCount * 3));
    }
    Profiler::get().endGPU("GBufferPass");

    cmd.endRenderPass();
    cmd.endLabel();
}

}
