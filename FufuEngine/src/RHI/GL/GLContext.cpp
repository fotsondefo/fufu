#include "depch.h"
#include "RHI/GL/GLContext.h"
#include "RHI/GL/GLResources.h"
#include "RHI/GL/GLPipeline.h"
#include "RHI/GL/GLSwapchain.h"
#include <stdexcept>
#include <cstdio>

namespace Fufu::RHI
{

// ── Debug callback GL ─────────────────────────────────────────────────────────

void GLAPIENTRY GLContext::debugCallback(GLenum /*source*/, GLenum type, GLuint id,
                                          GLenum severity, GLsizei /*length*/,
                                          const GLchar* message,
                                          const void* /*userParam*/)
{
    // Ignore les notifications
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    const char* typeStr =
        (type == GL_DEBUG_TYPE_ERROR)               ? "ERROR"      :
        (type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR) ? "DEPRECATED" :
        (type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)  ? "UB"         :
        (type == GL_DEBUG_TYPE_PERFORMANCE)         ? "PERF"       : "OTHER";

    fprintf(stderr, "[GL %s id=%u] %s\n", typeStr, id, message);
}

// ── Constructeur ──────────────────────────────────────────────────────────────

GLContext::GLContext(const ContextDesc& desc)
{
    m_Backend = Backend::OpenGL;

    if (desc.enableValidation)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                               GL_DEBUG_SEVERITY_NOTIFICATION,
                               0, nullptr, GL_FALSE);
    }
}

GLContext::~GLContext() = default;

// ── Resource factory ──────────────────────────────────────────────────────────

Ref<RHIBuffer> GLContext::createBuffer(const BufferDesc& d)
{
    return std::make_shared<GLBuffer>(d);
}

Ref<RHITexture> GLContext::createTexture(const TextureDesc& d)
{
    return std::make_shared<GLTexture>(d);
}

Ref<RHISampler> GLContext::createSampler(const SamplerDesc& d)
{
    return std::make_shared<GLSampler>(d);
}

Ref<RHIShader> GLContext::createShader(const ShaderDesc& d)
{
    return std::make_shared<GLShader>(d);
}

Ref<RHIPipeline> GLContext::createGraphicsPipeline(const GraphicsPipelineDesc& d)
{
    return std::make_shared<GLPipeline>(d);
}

Ref<RHIPipeline> GLContext::createComputePipeline(const ComputePipelineDesc& d)
{
    return std::make_shared<GLPipeline>(d);
}

Ref<RHISwapchain> GLContext::createSwapchain(const SwapchainDesc& d)
{
    return std::make_shared<GLSwapchain>(d);
}

// ── Frame ─────────────────────────────────────────────────────────────────────

RHICommandList* GLContext::beginFrame()
{
    m_CommandList.reset();
    return &m_CommandList;
}

void GLContext::endFrame()
{
    // GL : synchronisation implicite, pas de submit.
    // Le present() est géré par la swapchain.
    glFlush();
}

} // namespace Fufu::RHI
