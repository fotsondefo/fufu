#pragma once

#include "RHITypes.h"
#include "RHIBuffer.h"
#include "RHITexture.h"
#include "RHISampler.h"
#include "RHIShader.h"
#include "RHIPipeline.h"
#include "RHICommandList.h"
#include "RHISwapchain.h"

namespace Fufu::RHI
{

struct ContextDesc
{
    Backend  backend;
    void*    windowHandle     = nullptr;
    bool     enableValidation = false; // VK layers / GL debug callback
    uint32_t versionMajor    = 4;     // GL:4 / VK:1
    uint32_t versionMinor    = 3;     // GL:3 / VK:3
};

class RHIContext
{
public:
    virtual ~RHIContext() = default;

    // Instancie GLContext (OpenGL) ou VKContext (Vulkan) selon desc.backend.
    static Ref<RHIContext> create(const ContextDesc& desc);

    // ── Resource factory ──────────────────────────────────────────────────────
    virtual Ref<RHIBuffer>    createBuffer           (const BufferDesc&          d) = 0;
    virtual Ref<RHITexture>   createTexture          (const TextureDesc&         d) = 0;
    virtual Ref<RHISampler>   createSampler          (const SamplerDesc&         d) = 0;
    virtual Ref<RHIShader>    createShader           (const ShaderDesc&          d) = 0;
    virtual Ref<RHIPipeline>  createGraphicsPipeline (const GraphicsPipelineDesc& d) = 0;
    virtual Ref<RHIPipeline>  createComputePipeline  (const ComputePipelineDesc&  d) = 0;
    virtual Ref<RHISwapchain> createSwapchain        (const SwapchainDesc&       d) = 0;

    // ── Frame ─────────────────────────────────────────────────────────────────
    // beginFrame() retourne le CommandList prêt à enregistrer.
    // Un seul CommandList actif par frame (single-threaded recording).
    virtual RHICommandList* beginFrame() = 0;
    virtual void            endFrame()   = 0; // submit + synchronisation

    Backend getBackend() const { return m_Backend; }

protected:
    Backend m_Backend = Backend::OpenGL;
};

} // namespace Fufu::RHI
