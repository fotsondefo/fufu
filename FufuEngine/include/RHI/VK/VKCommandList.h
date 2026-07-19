#pragma once

#include "VKCommon.h"
#include "RHI/RHICommandList.h"
#include <array>

namespace Fufu::RHI
{

class VKCommandList final : public RHICommandList
{
public:
    VKCommandList() = default;

    // Appelé par VKContext au début de chaque frame
    void begin(VkCommandBuffer cmd, VkDescriptorSet descSet,
               VkDescriptorSetLayout layout, VkPipelineLayout pipelineLayout,
               VkDevice device);
    void end();

    // ── RHICommandList ────────────────────────────────────────────────────────
    void beginRenderPass(const RenderPassDesc& desc) override;
    void endRenderPass()                             override;

    void setViewport(float x, float y, float w, float h,
                     float minDepth = 0.f, float maxDepth = 1.f) override;
    void setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) override;
    void bindPipeline(RHIPipeline* pipeline) override;

    void bindUniformBuffer(uint32_t slot, RHIBuffer* buffer,
                           uint64_t offset = 0,
                           uint64_t size   = WHOLE_SIZE) override;
    void bindStorageBuffer(uint32_t slot, RHIBuffer* buffer,
                           uint64_t offset = 0,
                           uint64_t size   = WHOLE_SIZE) override;
    void bindTexture(uint32_t slot, RHITexture* texture,
                     RHISampler* sampler = nullptr) override;
    void bindStorageImage(uint32_t slot, RHITexture* texture) override;

    void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
              uint32_t firstVertex   = 0,
              uint32_t firstInstance = 0) override;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                     uint32_t firstIndex   = 0,
                     int32_t  vertexOffset  = 0,
                     uint32_t firstInstance = 0) override;
    void dispatch(uint32_t groupsX, uint32_t groupsY,
                  uint32_t groupsZ = 1) override;

    void barrier(const TextureBarrier& b) override;
    void beginLabel(const char* label, float r=1,float g=1,float b=1) override;
    void endLabel() override;

private:
    void flushDescriptors();
    void updateDescriptor(uint32_t binding, VkDescriptorType type,
                          VkBuffer buffer, VkDeviceSize offset,
                          VkDeviceSize range,
                          VkImageView imageView = VK_NULL_HANDLE,
                          VkSampler   sampler   = VK_NULL_HANDLE,
                          VkImageLayout layout  = VK_IMAGE_LAYOUT_UNDEFINED);

    VkDevice              m_Device        = VK_NULL_HANDLE;
    VkCommandBuffer       m_CommandBuffer = VK_NULL_HANDLE;
    VkDescriptorSet       m_DescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Layout        = VK_NULL_HANDLE;
    VkPipelineLayout      m_PipelineLayout = VK_NULL_HANDLE;
    bool                  m_DescDirty     = false;
    bool                  m_InsidePass    = false;

    // Pending descriptor writes
    std::vector<VkWriteDescriptorSet> m_PendingWrites;
    std::vector<VkDescriptorBufferInfo> m_BufferInfos;
    std::vector<VkDescriptorImageInfo>  m_ImageInfos;
};

} // namespace Fufu::RHI
