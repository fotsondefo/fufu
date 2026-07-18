#pragma once

#include "RHI/RHIContext.h"
#include "RHI/GL/GLCommandList.h"

namespace Fufu::RHI
{

class GLContext final : public RHIContext
{
public:
    explicit GLContext(const ContextDesc& desc);
    ~GLContext() override;

    // ── Resource factory ──────────────────────────────────────────────────────
    Ref<RHIBuffer>    createBuffer           (const BufferDesc&           d) override;
    Ref<RHITexture>   createTexture          (const TextureDesc&          d) override;
    Ref<RHISampler>   createSampler          (const SamplerDesc&          d) override;
    Ref<RHIShader>    createShader           (const ShaderDesc&           d) override;
    Ref<RHIPipeline>  createGraphicsPipeline (const GraphicsPipelineDesc& d) override;
    Ref<RHIPipeline>  createComputePipeline  (const ComputePipelineDesc&  d) override;
    Ref<RHISwapchain> createSwapchain        (const SwapchainDesc&        d) override;

    // ── Frame ─────────────────────────────────────────────────────────────────
    RHICommandList* beginFrame() override;
    void            endFrame()   override;

private:
    GLCommandList m_CommandList;

    static void debugCallback(GLenum source, GLenum type, GLuint id,
                               GLenum severity, GLsizei length,
                               const GLchar* message, const void* userParam);
};

} // namespace Fufu::RHI
