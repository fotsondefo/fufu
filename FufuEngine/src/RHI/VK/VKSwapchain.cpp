#include "RHI/VK/VKSwapchain.h"
#include <algorithm>
#include <stdexcept>
#include <limits>

// GLFW pour la surface (GLFW_INCLUDE_VULKAN est défini dans depch.h pour les TU VK)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Fufu::RHI
{

VKSwapchain::VKSwapchain(VkInstance instance, VkPhysicalDevice physDevice,
                         VkDevice device, uint32_t graphicsFamily,
                         const SwapchainDesc& desc)
    : m_Instance(instance), m_PhysDevice(physDevice),
      m_Device(device), m_GraphicsFamily(graphicsFamily), m_Desc(desc)
{
    auto* window = static_cast<GLFWwindow*>(desc.windowHandle);
    VK_CHECK(glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface));
    createSwapchain();

    // Semaphores par frame-in-flight
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        VK_CHECK(vkCreateSemaphore(m_Device, &sci, nullptr, &m_ImageAvailable[i]));
        VK_CHECK(vkCreateSemaphore(m_Device, &sci, nullptr, &m_RenderFinished[i]));
    }
}

VKSwapchain::~VKSwapchain()
{
    destroySwapchain();
    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        vkDestroySemaphore(m_Device, m_ImageAvailable[i], nullptr);
        vkDestroySemaphore(m_Device, m_RenderFinished[i], nullptr);
    }
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
}

// ── Création / destruction du swapchain ──────────────────────────────────────

void VKSwapchain::createSwapchain()
{
    // Support
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysDevice, m_Surface, &caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysDevice, m_Surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysDevice, m_Surface, &fmtCount, formats.data());

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysDevice, m_Surface, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysDevice, m_Surface, &pmCount, presentModes.data());

    m_SurfaceFormat = chooseSurfaceFormat(formats);
    VkPresentModeKHR presentMode = choosePresentMode(presentModes, m_Desc.vsync);

    VkExtent2D extent{};
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        extent = caps.currentExtent;
    else
        extent = { m_Desc.width, m_Desc.height };
    extent.width  = std::clamp(extent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    m_Desc.width  = extent.width;
    m_Desc.height = extent.height;

    uint32_t imageCount = std::max(m_Desc.imageCount, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = m_Surface;
    ci.minImageCount    = imageCount;
    ci.imageFormat      = m_SurfaceFormat.format;
    ci.imageColorSpace  = m_SurfaceFormat.colorSpace;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = presentMode;
    ci.clipped          = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(m_Device, &ci, nullptr, &m_Swapchain));

    // Récupère les images
    uint32_t cnt = 0;
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &cnt, nullptr);
    m_Images.resize(cnt);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &cnt, m_Images.data());

    m_ImageViews.resize(cnt);
    m_Textures.resize(cnt);
    for (uint32_t i = 0; i < cnt; ++i)
    {
        VkImageViewCreateInfo vci{};
        vci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image                           = m_Images[i];
        vci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vci.format                          = m_SurfaceFormat.format;
        vci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount     = 1;
        vci.subresourceRange.layerCount     = 1;
        VK_CHECK(vkCreateImageView(m_Device, &vci, nullptr, &m_ImageViews[i]));

        m_Textures[i] = std::make_unique<VKTexture>(
            m_Device, m_Images[i], m_ImageViews[i],
            extent.width, extent.height, m_SurfaceFormat.format);
    }
}

void VKSwapchain::destroySwapchain()
{
    m_Textures.clear();
    for (auto v : m_ImageViews) vkDestroyImageView(m_Device, v, nullptr);
    m_ImageViews.clear();
    m_Images.clear();
    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

void VKSwapchain::resize(uint32_t width, uint32_t height)
{
    m_Desc.width  = width;
    m_Desc.height = height;
    vkDeviceWaitIdle(m_Device);
    destroySwapchain();
    createSwapchain();
}

// ── Acquire / Present ─────────────────────────────────────────────────────────

RHITexture* VKSwapchain::acquireNextImage(VkSemaphore signalSemaphore)
{
    VkResult res = vkAcquireNextImageKHR(m_Device, m_Swapchain,
                                          UINT64_MAX, signalSemaphore,
                                          VK_NULL_HANDLE, &m_ImageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resize(m_Desc.width, m_Desc.height);
        return acquireNextImage(signalSemaphore);
    }
    VK_CHECK(res);
    return m_Textures[m_ImageIndex].get();
}

void VKSwapchain::present(VkQueue queue, VkSemaphore waitSemaphore)
{
    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &waitSemaphore;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &m_Swapchain;
    pi.pImageIndices      = &m_ImageIndex;

    VkResult res = vkQueuePresentKHR(queue, &pi);
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR && res != VK_ERROR_OUT_OF_DATE_KHR)
        VK_CHECK(res);
}

// Base-interface stubs — VKContext les court-circuite avec les surcharges VK
RHITexture* VKSwapchain::acquireNextImage()
{
    throw std::logic_error("VKSwapchain: use VKContext to drive acquire/present");
}
void VKSwapchain::present()
{
    throw std::logic_error("VKSwapchain: use VKContext to drive acquire/present");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

VkSurfaceFormatKHR VKSwapchain::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return formats[0];
}

VkPresentModeKHR VKSwapchain::choosePresentMode(
    const std::vector<VkPresentModeKHR>& modes, bool vsync)
{
    if (!vsync)
        for (auto m : modes)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

} // namespace Fufu::RHI
