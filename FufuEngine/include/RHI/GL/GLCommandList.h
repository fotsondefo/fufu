#pragma once

#include "RHI/RHICommandList.h"
#include <glad/glad.h>

namespace Fufu::RHI
{

class GLCommandList final : public RHICommandList
{
public:
    GLCommandList();
    ~GLCommandList() override;

    // ── Render pass ──────────────────────────────────────────────────────────
    void beginRenderPass(const RenderPassDesc& desc) override;
    void endRenderPass()                             override;

    // ── État rasterisation ───────────────────────────────────────────────────
    void setViewport(float x, float y, float w, float h,
                     float minDepth = 0.f, float maxDepth = 1.f) override;
    void setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) override;
    void bindPipeline(RHIPipeline* pipeline)                        override;

    // ── Ressources ───────────────────────────────────────────────────────────
    void bindUniformBuffer(uint32_t slot, RHIBuffer* buffer,
                           uint64_t offset = 0,
                           uint64_t size   = WHOLE_SIZE) override;
    void bindStorageBuffer(uint32_t slot, RHIBuffer* buffer,
                           uint64_t offset = 0,
                           uint64_t size   = WHOLE_SIZE) override;
    void bindTexture(uint32_t slot, RHITexture* texture,
                     RHISampler* sampler = nullptr)     override;
    void bindStorageImage(uint32_t slot, RHITexture* texture) override;

    // ── Draw / Dispatch ───────────────────────────────────────────────────────
    void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
              uint32_t firstVertex   = 0,
              uint32_t firstInstance = 0) override;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                     uint32_t firstIndex   = 0,
                     int32_t  vertexOffset  = 0,
                     uint32_t firstInstance = 0) override;
    void dispatch(uint32_t groupsX, uint32_t groupsY,
                  uint32_t groupsZ = 1) override;

    // ── Synchronisation ───────────────────────────────────────────────────────
    void barrier(const TextureBarrier& b) override;

    // ── Debug ─────────────────────────────────────────────────────────────────
    void beginLabel(const char* label, float r=1,float g=1,float b=1) override;
    void endLabel()                                                      override;

    // Réinitialise l'état interne en début de frame
    void reset();

private:
    // FBO persistant : ré-attaché à chaque beginRenderPass()
    GLuint m_FBO         = 0;
    GLuint m_DummyVAO    = 0; // VAO vide requis par le core profile

    RHIPipeline* m_BoundPipeline = nullptr;

    void attachColorTexture (GLenum attachment, RHITexture* tex);
    void attachDepthTexture (RHITexture* tex);
};

} // namespace Fufu::RHI
