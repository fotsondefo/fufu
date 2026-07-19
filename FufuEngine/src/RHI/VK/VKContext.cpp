#include "RHI/VK/VKContext.h"
#include "RHI/VK/VKResources.h"
#include "RHI/VK/VKPipeline.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <cstring>

namespace Fufu::RHI
{

// ── Debug messenger ───────────────────────────────────────────────────────────

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        spdlog::warn("[VK] {}", data->pMessage);
    return VK_FALSE;
}

// ── Constructeur / Destructeur ────────────────────────────────────────────────

VKContext::VKContext(const ContextDesc& desc)
{
    m_Backend = Backend::Vulkan;
    bool validation = desc.enableValidation;

    createInstance(validation);
    if (validation) createDebugMessenger();
    pickPhysicalDevice();
    createDevice();
    createAllocator();
    createCommandPool();
    createDescriptorSetLayout();
    createPipelineLayout();
    createFrameResources();
}

VKContext::~VKContext()
{
    vkDeviceWaitIdle(m_Device);
    destroyFrameResources();
    if (m_PipelineLayout  != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    if (m_DescSetLayout   != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_Device, m_DescSetLayout, nullptr);
    if (m_CommandPool     != VK_NULL_HANDLE) vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    if (m_Allocator       != VK_NULL_HANDLE) vmaDestroyAllocator(m_Allocator);
    if (m_Device          != VK_NULL_HANDLE) vkDestroyDevice(m_Device, nullptr);
    if (m_DebugMessenger  != VK_NULL_HANDLE)
    {
        auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn) fn(m_Instance, m_DebugMessenger, nullptr);
    }
    if (m_Instance != VK_NULL_HANDLE) vkDestroyInstance(m_Instance, nullptr);
}

// ── createInstance ────────────────────────────────────────────────────────────

void VKContext::createInstance(bool validation)
{
    VkApplicationInfo ai{};
    ai.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "FufuEngine";
    ai.apiVersion       = VK_API_VERSION_1_3;

    // Extensions requises par GLFW + debug
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> exts(glfwExts, glfwExts + glfwExtCount);
    if (validation) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const char* layer = "VK_LAYER_KHRONOS_validation";

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &ai;
    ci.enabledExtensionCount   = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();
    if (validation)
    {
        ci.enabledLayerCount   = 1;
        ci.ppEnabledLayerNames = &layer;
    }

    VK_CHECK(vkCreateInstance(&ci, nullptr, &m_Instance));
}

// ── createDebugMessenger ──────────────────────────────────────────────────────

void VKContext::createDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debugCallback;

    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
    if (fn) VK_CHECK(fn(m_Instance, &ci, nullptr, &m_DebugMessenger));
}

// ── pickPhysicalDevice ────────────────────────────────────────────────────────

void VKContext::pickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan physical devices found");

    std::vector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(m_Instance, &count, devs.data());

    for (auto dev : devs)
    {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qProps.data());

        for (uint32_t i = 0; i < qCount; ++i)
        {
            if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                m_PhysDevice     = dev;
                m_GraphicsFamily = i;
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(dev, &props);
                spdlog::info("[VK] GPU: {}", props.deviceName);
                return;
            }
        }
    }
    throw std::runtime_error("No suitable Vulkan GPU found");
}

// ── createDevice ─────────────────────────────────────────────────────────────

void VKContext::createDevice()
{
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = m_GraphicsFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &prio;

    // Extensions requises
    const std::vector<const char*> devExts = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    // Features Vulkan 1.2 : descriptor indexing (update-after-bind)
    VkPhysicalDeviceDescriptorIndexingFeatures diFeats{};
    diFeats.sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    diFeats.descriptorBindingPartiallyBound              = VK_TRUE;
    diFeats.descriptorBindingUpdateUnusedWhilePending    = VK_TRUE;
    diFeats.descriptorBindingUniformBufferUpdateAfterBind  = VK_TRUE;
    diFeats.descriptorBindingStorageBufferUpdateAfterBind  = VK_TRUE;
    diFeats.descriptorBindingSampledImageUpdateAfterBind   = VK_TRUE;
    diFeats.descriptorBindingStorageImageUpdateAfterBind   = VK_TRUE;
    diFeats.runtimeDescriptorArray                         = VK_TRUE;

    // Features Vulkan 1.3 : dynamic rendering + synchronization2
    VkPhysicalDeviceDynamicRenderingFeatures dynRender{};
    dynRender.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynRender.dynamicRendering = VK_TRUE;
    dynRender.pNext            = &diFeats;

    VkPhysicalDeviceSynchronization2Features sync2Feats{};
    sync2Feats.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2Feats.synchronization2 = VK_TRUE;
    sync2Feats.pNext            = &dynRender;

    // Features de base (anisotropy)
    VkPhysicalDeviceFeatures baseFeats{};
    baseFeats.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo dci{};
    dci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext                   = &sync2Feats;
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = (uint32_t)devExts.size();
    dci.ppEnabledExtensionNames = devExts.data();
    dci.pEnabledFeatures        = &baseFeats;

    VK_CHECK(vkCreateDevice(m_PhysDevice, &dci, nullptr, &m_Device));
    vkGetDeviceQueue(m_Device, m_GraphicsFamily, 0, &m_GraphicsQueue);
}

// ── createAllocator ───────────────────────────────────────────────────────────

void VKContext::createAllocator()
{
    VmaAllocatorCreateInfo ai{};
    ai.physicalDevice   = m_PhysDevice;
    ai.device           = m_Device;
    ai.instance         = m_Instance;
    ai.vulkanApiVersion = VK_API_VERSION_1_3;
    VK_CHECK(vmaCreateAllocator(&ai, &m_Allocator));
}

// ── createCommandPool ─────────────────────────────────────────────────────────

void VKContext::createCommandPool()
{
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = m_GraphicsFamily;
    VK_CHECK(vkCreateCommandPool(m_Device, &ci, nullptr, &m_CommandPool));
}

// ── createDescriptorSetLayout ─────────────────────────────────────────────────

void VKContext::createDescriptorSetLayout()
{
    // 64 bindings : 0-15 UBO, 16-31 SSBO, 32-63 CombinedImageSampler
    std::vector<VkDescriptorSetLayoutBinding>     bindings(kDescTotalSlots);
    std::vector<VkDescriptorBindingFlags>         flags(kDescTotalSlots,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

    for (uint32_t i = 0; i < kDescTotalSlots; ++i)
    {
        VkDescriptorType type;
        if      (i < kDescSSBOBase)    type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        else if (i < kDescSamplerBase) type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        else                            type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        bindings[i].binding         = i;
        bindings[i].descriptorType  = type;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags      = VK_SHADER_STAGE_ALL;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCI{};
    flagsCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsCI.bindingCount  = kDescTotalSlots;
    flagsCI.pBindingFlags = flags.data();

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.pNext        = &flagsCI;
    ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    ci.bindingCount = kDescTotalSlots;
    ci.pBindings    = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(m_Device, &ci, nullptr, &m_DescSetLayout));
}

// ── createPipelineLayout ──────────────────────────────────────────────────────

void VKContext::createPipelineLayout()
{
    VkPipelineLayoutCreateInfo ci{};
    ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount = 1;
    ci.pSetLayouts    = &m_DescSetLayout;
    VK_CHECK(vkCreatePipelineLayout(m_Device, &ci, nullptr, &m_PipelineLayout));
}

// ── createFrameResources ──────────────────────────────────────────────────────

void VKContext::createFrameResources()
{
    // Pool de descripteurs avec UPDATE_AFTER_BIND
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         16 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         16 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32 },
    };

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolCI.maxSets       = 1;
    poolCI.poolSizeCount = 3;
    poolCI.pPoolSizes    = poolSizes;

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool        = m_CommandPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
                           VK_FENCE_CREATE_SIGNALED_BIT };

    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        auto& f = m_Frames[i];
        VK_CHECK(vkAllocateCommandBuffers(m_Device, &cbai, &f.commandBuffer));
        VK_CHECK(vkCreateFence(m_Device, &fci, nullptr, &f.fence));
        VK_CHECK(vkCreateDescriptorPool(m_Device, &poolCI, nullptr, &f.descPool));

        VkDescriptorSetAllocateInfo dsai{};
        dsai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool     = f.descPool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts        = &m_DescSetLayout;
        VK_CHECK(vkAllocateDescriptorSets(m_Device, &dsai, &f.descSet));
    }
}

void VKContext::destroyFrameResources()
{
    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        auto& f = m_Frames[i];
        if (f.fence     != VK_NULL_HANDLE) vkDestroyFence(m_Device, f.fence, nullptr);
        if (f.descPool  != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_Device, f.descPool, nullptr);
        // commandBuffer appartient au pool — pas besoin de free individuel
    }
}

// ── Factories ─────────────────────────────────────────────────────────────────

Ref<RHIBuffer> VKContext::createBuffer(const BufferDesc& d)
{
    return std::make_shared<VKBuffer>(m_Device, m_Allocator, d);
}

Ref<RHITexture> VKContext::createTexture(const TextureDesc& d)
{
    return std::make_shared<VKTexture>(m_Device, m_Allocator, d);
}

Ref<RHISampler> VKContext::createSampler(const SamplerDesc& d)
{
    return std::make_shared<VKSampler>(m_Device, d);
}

Ref<RHIShader> VKContext::createShader(const ShaderDesc& d)
{
    return std::make_shared<VKShader>(m_Device, d);
}

Ref<RHIPipeline> VKContext::createGraphicsPipeline(const GraphicsPipelineDesc& d)
{
    return std::make_shared<VKPipeline>(m_Device, m_PipelineLayout, d);
}

Ref<RHIPipeline> VKContext::createComputePipeline(const ComputePipelineDesc& d)
{
    return std::make_shared<VKPipeline>(m_Device, m_PipelineLayout, d);
}

Ref<RHISwapchain> VKContext::createSwapchain(const SwapchainDesc& d)
{
    auto sc = std::make_shared<VKSwapchain>(
        m_Instance, m_PhysDevice, m_Device, m_GraphicsFamily, d);
    m_Swapchain = sc.get();
    return sc;
}

// ── Frame loop ────────────────────────────────────────────────────────────────

RHICommandList* VKContext::beginFrame()
{
    auto& frame = m_Frames[m_FrameIdx];

    // Attend la fin de la frame précédente sur ce slot
    VK_CHECK(vkWaitForFences(m_Device, 1, &frame.fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(m_Device, 1, &frame.fence));

    // Acquiert l'image swapchain (si swapchain présent)
    if (m_Swapchain)
    {
        VkSemaphore imgAvail = m_Swapchain->getImageAvailableSemaphore(m_FrameIdx);
        m_Swapchain->acquireNextImage(imgAvail);
    }

    // Commence l'enregistrement
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkResetCommandBuffer(frame.commandBuffer, 0));
    VK_CHECK(vkBeginCommandBuffer(frame.commandBuffer, &bi));

    m_CommandList.begin(frame.commandBuffer, frame.descSet,
                        m_DescSetLayout, m_PipelineLayout, m_Device);
    return &m_CommandList;
}

void VKContext::endFrame()
{
    m_CommandList.end();

    auto& frame = m_Frames[m_FrameIdx];

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSems[1]   = {};
    VkSemaphore signalSems[1] = {};
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (m_Swapchain)
    {
        waitSems[0]   = m_Swapchain->getImageAvailableSemaphore(m_FrameIdx);
        signalSems[0] = m_Swapchain->getRenderFinishedSemaphore(m_FrameIdx);
        si.waitSemaphoreCount   = 1;
        si.pWaitSemaphores      = waitSems;
        si.pWaitDstStageMask    = &waitStage;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores    = signalSems;
    }

    si.commandBufferCount = 1;
    si.pCommandBuffers    = &frame.commandBuffer;

    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &si, frame.fence));

    if (m_Swapchain)
        m_Swapchain->present(m_GraphicsQueue,
                             m_Swapchain->getRenderFinishedSemaphore(m_FrameIdx));

    m_FrameIdx = (m_FrameIdx + 1) % kFramesInFlight;
}

// ── Utilitaires ───────────────────────────────────────────────────────────────

void VKContext::transitionImage(VkCommandBuffer cmd, VkImage image,
                                VkImageLayout from, VkImageLayout to,
                                VkImageAspectFlags aspect)
{
    VkImageMemoryBarrier2 bar{};
    bar.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    bar.srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    bar.srcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    bar.dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    bar.dstAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    bar.oldLayout        = from;
    bar.newLayout        = to;
    bar.image            = image;
    bar.subresourceRange = { aspect, 0, 1, 0, 1 };

    VkDependencyInfo dep{};
    dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers    = &bar;
    vkCmdPipelineBarrier2(cmd, &dep);
}

void VKContext::immediateSubmit(std::function<void(VkCommandBuffer)>&& fn)
{
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = m_CommandPool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(m_Device, &ai, &cmd));

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
    fn(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(m_Device, &fci, nullptr, &fence));
    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &si, fence));
    VK_CHECK(vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_Device, fence, nullptr);
    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);
}

} // namespace Fufu::RHI
