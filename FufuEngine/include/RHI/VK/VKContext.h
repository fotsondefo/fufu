#pragma once

#include "VKCommon.h"
#include "VKCommandList.h"
#include "VKSwapchain.h"
#include "RHI/RHIContext.h"
#include <array>

namespace Fufu::RHI
{

class VKContext final : public RHIContext
{
public:
    explicit VKContext(const ContextDesc& desc);
    ~VKContext() override;

    // ── Factories ────────────────────────────────────────────────────────────
    Ref<RHIBuffer>    createBuffer(const BufferDesc& d)   override;
    Ref<RHITexture>   createTexture(const TextureDesc& d) override;
    Ref<RHISampler>   createSampler(const SamplerDesc& d) override;
    Ref<RHIShader>    createShader(const ShaderDesc& d)   override;
    Ref<RHIPipeline>  createGraphicsPipeline(const GraphicsPipelineDesc& d) override;
    Ref<RHIPipeline>  createComputePipeline(const ComputePipelineDesc& d)   override;
    Ref<RHISwapchain> createSwapchain(const SwapchainDesc& d) override;

    // ── Frame loop ───────────────────────────────────────────────────────────
    RHICommandList* beginFrame() override;
    void            endFrame()   override;

    // ── Accessors internes ───────────────────────────────────────────────────
    VkDevice         getDevice()    const { return m_Device; }
    VkPhysicalDevice getPhysical()  const { return m_PhysDevice; }
    VmaAllocator     getAllocator()  const { return m_Allocator; }
    VkQueue          getGfxQueue()  const { return m_GraphicsQueue; }
    uint32_t         getGfxFamily() const { return m_GraphicsFamily; }
    VkInstance       getInstance()  const { return m_Instance; }
    VkPipelineLayout getPipelineLayout() const { return m_PipelineLayout; }

    // ── Utilitaires ──────────────────────────────────────────────────────────
    void transitionImage(VkCommandBuffer cmd, VkImage image,
                         VkImageLayout from, VkImageLayout to,
                         VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void immediateSubmit(std::function<void(VkCommandBuffer)>&& fn);

private:
    void createInstance(bool validation);
    void createDebugMessenger();
    void pickPhysicalDevice();
    void createDevice();
    void createAllocator();
    void createCommandPool();
    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createFrameResources();
    void destroyFrameResources();

    // ── Vulkan objets ────────────────────────────────────────────────────────
    VkInstance               m_Instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice         m_PhysDevice     = VK_NULL_HANDLE;
    VkDevice                 m_Device         = VK_NULL_HANDLE;
    VkQueue                  m_GraphicsQueue  = VK_NULL_HANDLE;
    uint32_t                 m_GraphicsFamily = 0;
    VmaAllocator             m_Allocator      = VK_NULL_HANDLE;
    VkCommandPool            m_CommandPool    = VK_NULL_HANDLE;
    VkDescriptorSetLayout    m_DescSetLayout  = VK_NULL_HANDLE;
    VkPipelineLayout         m_PipelineLayout = VK_NULL_HANDLE;

    // ── Frame in flight ──────────────────────────────────────────────────────
    struct FrameData
    {
        VkCommandBuffer  commandBuffer  = VK_NULL_HANDLE;
        VkFence          fence          = VK_NULL_HANDLE;
        VkDescriptorPool descPool       = VK_NULL_HANDLE;
        VkDescriptorSet  descSet        = VK_NULL_HANDLE;
    };
    std::array<FrameData, kFramesInFlight> m_Frames{};
    uint32_t m_FrameIdx = 0;

    VKCommandList m_CommandList;
    VKSwapchain*  m_Swapchain = nullptr; // non-owning, updated when swapchain is created
};

} // namespace Fufu::RHI
