#pragma once

#include "VKCommon.h"
#include "VKResources.h"
#include "RHI/RHISwapchain.h"
#include <vector>
#include <memory>

namespace Fufu::RHI
{

class VKSwapchain final : public RHISwapchain
{
public:
    VKSwapchain(VkInstance instance, VkPhysicalDevice physDevice,
                VkDevice device, uint32_t graphicsFamily,
                const SwapchainDesc& desc);
    ~VKSwapchain() override;

    void        resize(uint32_t width, uint32_t height) override;

    // Base interface (non appelée directement — VKContext utilise les surcharges VK)
    RHITexture* acquireNextImage() override;
    void        present()          override;

    // Surcharges VK avec paramètres de synchronisation
    RHITexture* acquireNextImage(VkSemaphore signalSemaphore);
    void        present(VkQueue queue, VkSemaphore waitSemaphore);

    uint32_t getWidth()  const override { return m_Desc.width;  }
    uint32_t getHeight() const override { return m_Desc.height; }
    VkFormat getFormat() const          { return m_SurfaceFormat.format; }
    uint32_t getImageCount() const      { return static_cast<uint32_t>(m_Images.size()); }
    uint32_t getCurrentImageIndex() const { return m_ImageIndex; }

    VkSemaphore getImageAvailableSemaphore(uint32_t frameIdx) const
        { return m_ImageAvailable[frameIdx]; }
    VkSemaphore getRenderFinishedSemaphore(uint32_t frameIdx) const
        { return m_RenderFinished[frameIdx]; }

private:
    void createSwapchain();
    void destroySwapchain();
    static VkSurfaceFormatKHR chooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& formats);
    static VkPresentModeKHR choosePresentMode(
        const std::vector<VkPresentModeKHR>& modes, bool vsync);

    VkInstance       m_Instance;
    VkPhysicalDevice m_PhysDevice;
    VkDevice         m_Device;
    uint32_t         m_GraphicsFamily;
    SwapchainDesc    m_Desc;

    VkSurfaceKHR      m_Surface       = VK_NULL_HANDLE;
    VkSwapchainKHR    m_Swapchain     = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_SurfaceFormat{};
    uint32_t           m_ImageIndex   = 0;

    std::vector<VkImage>                   m_Images;
    std::vector<VkImageView>               m_ImageViews;
    std::vector<std::unique_ptr<VKTexture>> m_Textures;

    // Sync per frame-in-flight
    VkSemaphore m_ImageAvailable[kFramesInFlight] = {};
    VkSemaphore m_RenderFinished[kFramesInFlight] = {};
};

} // namespace Fufu::RHI
