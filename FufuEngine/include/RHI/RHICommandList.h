#pragma once

#include "RHITypes.h"
#include "RHIBuffer.h"
#include "RHITexture.h"
#include "RHISampler.h"
#include "RHIPipeline.h"
#include <vector>
#include <optional>
#include <array>

namespace Fufu::RHI
{

// ── Render pass inline ───────────────────────────────────────────────────────
enum class LoadOp  { Load, Clear, DontCare };
enum class StoreOp { Store, DontCare };

struct ColorAttachment
{
    Ref<RHITexture>      texture;
    LoadOp               loadOp     = LoadOp::Clear;
    StoreOp              storeOp    = StoreOp::Store;
    std::array<float, 4> clearColor = { 0.f, 0.f, 0.f, 1.f };
};

struct DepthAttachment
{
    Ref<RHITexture> texture;
    LoadOp          loadOp     = LoadOp::Clear;
    StoreOp         storeOp    = StoreOp::DontCare;
    float           clearDepth = 1.0f;
};

struct RenderPassDesc
{
    std::vector<ColorAttachment>  colorAttachments;
    std::optional<DepthAttachment> depthAttachment;
    uint32_t width  = 0;
    uint32_t height = 0;
};

// ── Barrière de layout (no-op GL, VkPipelineBarrier2 côté VK) ────────────────
enum class TextureLayout
{
    Undefined,
    ColorAttachment,
    DepthAttachment,
    ShaderReadOnly,
    StorageImage,
    Present,
    TransferSrc,
    TransferDst,
};

struct TextureBarrier
{
    Ref<RHITexture> texture;
    TextureLayout   from;
    TextureLayout   to;
};

// ── Interface CommandList ────────────────────────────────────────────────────
class RHICommandList
{
public:
    virtual ~RHICommandList() = default;

    // ── Render pass ──────────────────────────────────────────────────────────
    virtual void beginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void endRenderPass()                             = 0;

    // ── État rasterisation ───────────────────────────────────────────────────
    virtual void setViewport(float x, float y, float w, float h,
                              float minDepth = 0.f, float maxDepth = 1.f) = 0;
    virtual void setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;
    virtual void bindPipeline(RHIPipeline* pipeline)                        = 0;

    // ── Ressources (slot = binding point GL / descriptor binding VK) ─────────
    virtual void bindUniformBuffer(uint32_t slot, RHIBuffer* buffer,
                                    uint64_t offset = 0,
                                    uint64_t size   = WHOLE_SIZE)           = 0;
    virtual void bindStorageBuffer(uint32_t slot, RHIBuffer* buffer,
                                    uint64_t offset = 0,
                                    uint64_t size   = WHOLE_SIZE)           = 0;
    virtual void bindTexture(uint32_t slot, RHITexture* texture,
                              RHISampler* sampler = nullptr)                 = 0;
    virtual void bindStorageImage(uint32_t slot, RHITexture* texture)       = 0;

    // ── Draw / Dispatch ───────────────────────────────────────────────────────
    virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
                       uint32_t firstVertex   = 0,
                       uint32_t firstInstance = 0)                          = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                              uint32_t firstIndex   = 0,
                              int32_t  vertexOffset  = 0,
                              uint32_t firstInstance = 0)                   = 0;
    virtual void dispatch(uint32_t groupsX, uint32_t groupsY,
                           uint32_t groupsZ = 1)                            = 0;

    // ── Synchronisation ───────────────────────────────────────────────────────
    // GL : glMemoryBarrier ; VK : vkCmdPipelineBarrier2
    virtual void barrier(const TextureBarrier& b) = 0;

    // ── Debug labels (RenderDoc / Nsight) ─────────────────────────────────────
    virtual void beginLabel(const char* label, float r=1,float g=1,float b=1) = 0;
    virtual void endLabel()                                                    = 0;

    // WHOLE_SIZE est défini dans RHIBuffer.h (inclus via RHIBuffer.h)
};

} // namespace Fufu::RHI
