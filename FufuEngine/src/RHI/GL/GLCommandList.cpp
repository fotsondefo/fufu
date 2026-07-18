#include "depch.h"
#include "RHI/GL/GLCommandList.h"
#include "RHI/GL/GLResources.h"
#include "RHI/GL/GLPipeline.h"

namespace Fufu::RHI
{

GLCommandList::GLCommandList()
{
    glGenFramebuffers(1, &m_FBO);
    glGenVertexArrays(1, &m_DummyVAO);
    glBindVertexArray(m_DummyVAO);
}

GLCommandList::~GLCommandList()
{
    if (m_FBO)      glDeleteFramebuffers(1, &m_FBO);
    if (m_DummyVAO) glDeleteVertexArrays(1, &m_DummyVAO);
}

void GLCommandList::reset()
{
    m_BoundPipeline = nullptr;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void GLCommandList::attachColorTexture(GLenum attachment, RHITexture* tex)
{
    auto* glTex = static_cast<GLTexture*>(tex);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                            glTex->getGLTarget(), glTex->getHandle(), 0);
}

void GLCommandList::attachDepthTexture(RHITexture* tex)
{
    auto* glTex = static_cast<GLTexture*>(tex);
    GLenum attachment = GL_DEPTH_ATTACHMENT;
    if (glTex->getGLInternalFmt() == GL_DEPTH24_STENCIL8)
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                            glTex->getGLTarget(), glTex->getHandle(), 0);
}

// ── Render pass ───────────────────────────────────────────────────────────────

void GLCommandList::beginRenderPass(const RenderPassDesc& desc)
{
    const bool useDefaultFBO = desc.colorAttachments.empty() ||
                               !desc.colorAttachments[0].texture;

    if (useDefaultFBO)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // Attache les color textures
        std::vector<GLenum> drawBuffers;
        for (uint32_t i = 0; i < static_cast<uint32_t>(desc.colorAttachments.size()); ++i)
        {
            const auto& att = desc.colorAttachments[i];
            GLenum slot = GL_COLOR_ATTACHMENT0 + i;
            attachColorTexture(slot, att.texture.get());
            drawBuffers.push_back(slot);
        }
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());

        // Attache depth
        if (desc.depthAttachment && desc.depthAttachment->texture)
            attachDepthTexture(desc.depthAttachment->texture.get());
    }

    // Viewport implicite
    glViewport(0, 0,
               static_cast<GLsizei>(desc.width),
               static_cast<GLsizei>(desc.height));

    // Clear
    GLbitfield clearMask = 0;

    for (const auto& att : desc.colorAttachments)
    {
        if (att.loadOp == LoadOp::Clear)
        {
            clearMask |= GL_COLOR_BUFFER_BIT;
            glClearColor(att.clearColor[0], att.clearColor[1],
                          att.clearColor[2], att.clearColor[3]);
            break; // glClearColor s'applique à tous les color attachments
        }
    }

    if (desc.depthAttachment && desc.depthAttachment->loadOp == LoadOp::Clear)
    {
        clearMask |= GL_DEPTH_BUFFER_BIT;
        glClearDepth(static_cast<GLdouble>(desc.depthAttachment->clearDepth));
        glDepthMask(GL_TRUE);
    }

    if (clearMask) glClear(clearMask);
}

void GLCommandList::endRenderPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── État ──────────────────────────────────────────────────────────────────────

void GLCommandList::setViewport(float x, float y, float w, float h,
                                 float /*minDepth*/, float /*maxDepth*/)
{
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
               static_cast<GLsizei>(w), static_cast<GLsizei>(h));
}

void GLCommandList::setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
}

void GLCommandList::bindPipeline(RHIPipeline* pipeline)
{
    m_BoundPipeline = pipeline;
    static_cast<GLPipeline*>(pipeline)->applyState();
}

// ── Ressources ────────────────────────────────────────────────────────────────

void GLCommandList::bindUniformBuffer(uint32_t slot, RHIBuffer* buffer,
                                       uint64_t offset, uint64_t size)
{
    auto* glBuf = static_cast<GLBuffer*>(buffer);
    if (size == WHOLE_SIZE)
        glBindBufferBase(GL_UNIFORM_BUFFER, slot, glBuf->getHandle());
    else
        glBindBufferRange(GL_UNIFORM_BUFFER, slot, glBuf->getHandle(),
                           static_cast<GLintptr>(offset),
                           static_cast<GLsizeiptr>(size));
}

void GLCommandList::bindStorageBuffer(uint32_t slot, RHIBuffer* buffer,
                                       uint64_t offset, uint64_t size)
{
    auto* glBuf = static_cast<GLBuffer*>(buffer);
    if (size == WHOLE_SIZE)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, glBuf->getHandle());
    else
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, slot, glBuf->getHandle(),
                           static_cast<GLintptr>(offset),
                           static_cast<GLsizeiptr>(size));
}

void GLCommandList::bindTexture(uint32_t slot, RHITexture* texture,
                                 RHISampler* sampler)
{
    auto* glTex = static_cast<GLTexture*>(texture);
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(glTex->getGLTarget(), glTex->getHandle());
    if (sampler)
    {
        auto* glSam = static_cast<GLSampler*>(sampler);
        glBindSampler(slot, glSam->getHandle());
    }
}

void GLCommandList::bindStorageImage(uint32_t slot, RHITexture* texture)
{
    auto* glTex = static_cast<GLTexture*>(texture);
    glBindImageTexture(slot, glTex->getHandle(), 0, GL_FALSE, 0,
                        GL_READ_WRITE, glTex->getGLInternalFmt());
}

// ── Draw / Dispatch ───────────────────────────────────────────────────────────

void GLCommandList::draw(uint32_t vertexCount, uint32_t instanceCount,
                          uint32_t firstVertex, uint32_t firstInstance)
{
    if (instanceCount > 1 || firstInstance > 0)
        glDrawArraysInstancedBaseInstance(GL_TRIANGLES,
                                           static_cast<GLint>(firstVertex),
                                           static_cast<GLsizei>(vertexCount),
                                           static_cast<GLsizei>(instanceCount),
                                           firstInstance);
    else
        glDrawArrays(GL_TRIANGLES,
                      static_cast<GLint>(firstVertex),
                      static_cast<GLsizei>(vertexCount));
}

void GLCommandList::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                 uint32_t firstIndex, int32_t vertexOffset,
                                 uint32_t firstInstance)
{
    const void* offset = reinterpret_cast<const void*>(
        static_cast<uintptr_t>(firstIndex) * sizeof(uint32_t));

    if (instanceCount > 1 || firstInstance > 0)
        glDrawElementsInstancedBaseVertexBaseInstance(
            GL_TRIANGLES, static_cast<GLsizei>(indexCount),
            GL_UNSIGNED_INT, offset,
            static_cast<GLsizei>(instanceCount),
            vertexOffset, firstInstance);
    else
        glDrawElementsBaseVertex(GL_TRIANGLES,
                                  static_cast<GLsizei>(indexCount),
                                  GL_UNSIGNED_INT, offset, vertexOffset);
}

void GLCommandList::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    glDispatchCompute(groupsX, groupsY, groupsZ);
}

// ── Synchronisation ───────────────────────────────────────────────────────────

void GLCommandList::barrier(const TextureBarrier& b)
{
    GLbitfield mask = GL_ALL_BARRIER_BITS;
    switch (b.to)
    {
    case TextureLayout::ShaderReadOnly:
        mask = GL_TEXTURE_FETCH_BARRIER_BIT; break;
    case TextureLayout::StorageImage:
        mask = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT; break;
    case TextureLayout::ColorAttachment:
        mask = GL_FRAMEBUFFER_BARRIER_BIT; break;
    default: break;
    }
    glMemoryBarrier(mask);
}

// ── Debug ─────────────────────────────────────────────────────────────────────

void GLCommandList::beginLabel(const char* label, float /*r*/, float /*g*/, float /*b*/)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
}

void GLCommandList::endLabel()
{
    glPopDebugGroup();
}

} // namespace Fufu::RHI
