#pragma once

#include "VKCommon.h"
#include "RHI/RHIBuffer.h"
#include "RHI/RHITexture.h"
#include "RHI/RHISampler.h"
#include "RHI/RHIShader.h"

namespace Fufu::RHI
{

// ── VKBuffer ─────────────────────────────────────────────────────────────────
class VKBuffer final : public RHIBuffer
{
public:
    VKBuffer(VkDevice device, VmaAllocator allocator, const BufferDesc& desc);
    ~VKBuffer() override;

    void   upload(const void* data, uint64_t size, uint64_t offset = 0) override;
    void*  map(uint64_t offset = 0, uint64_t size = WHOLE_SIZE) override;
    void   unmap() override;

    VkBuffer      getBuffer()    const { return m_Buffer; }
    VmaAllocation getAllocation() const { return m_Allocation; }

private:
    VkDevice      m_Device;
    VmaAllocator  m_Allocator;
    VkBuffer      m_Buffer      = VK_NULL_HANDLE;
    VmaAllocation m_Allocation  = VK_NULL_HANDLE;
    bool          m_IsMapped    = false;
};

// ── VKTexture ────────────────────────────────────────────────────────────────
class VKTexture final : public RHITexture
{
public:
    // Texture normale (allouée par VMA)
    VKTexture(VkDevice device, VmaAllocator allocator, const TextureDesc& desc);
    // Wrapper image swapchain (pas de VMA, pas de destruction d'image)
    VKTexture(VkDevice device, VkImage image, VkImageView view,
              uint32_t width, uint32_t height, VkFormat format);
    ~VKTexture() override;

    void upload(const void* data, uint32_t mip = 0, uint32_t layer = 0) override;
    uint64_t getNativeHandle() const override { return reinterpret_cast<uint64_t>(m_Image); }

    VkImage     getImage()     const { return m_Image; }
    VkImageView getImageView() const { return m_ImageView; }
    VkFormat    getVKFormat()  const { return m_VKFormat; }

    VkImageLayout getCurrentLayout() const { return m_CurrentLayout; }
    void          setCurrentLayout(VkImageLayout l) { m_CurrentLayout = l; }

    bool isSwapchainImage() const { return m_IsSwapchain; }

private:
    void createImageView();
    static VkFormat toVKFormat(Format f);
    static VkImageAspectFlags aspectOf(VkFormat fmt);

    VkDevice      m_Device;
    VmaAllocator  m_Allocator  = VK_NULL_HANDLE;
    VkImage       m_Image      = VK_NULL_HANDLE;
    VkImageView   m_ImageView  = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    VkFormat      m_VKFormat   = VK_FORMAT_UNDEFINED;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool          m_IsSwapchain   = false;
};

// ── VKSampler ────────────────────────────────────────────────────────────────
class VKSampler final : public RHISampler
{
public:
    VKSampler(VkDevice device, const SamplerDesc& desc);
    ~VKSampler() override;

    VkSampler getSampler() const { return m_Sampler; }

private:
    VkDevice  m_Device;
    VkSampler m_Sampler = VK_NULL_HANDLE;
};

// ── VKShader ─────────────────────────────────────────────────────────────────
class VKShader final : public RHIShader
{
public:
    VKShader(VkDevice device, const ShaderDesc& desc);
    ~VKShader() override;

    VkShaderModule getModule()     const { return m_Module; }
    ShaderStage    getStage()      const { return m_Desc.stage; }
    const char*    getEntryPoint() const { return m_Desc.entryPoint.c_str(); }

private:
    static std::vector<uint32_t> compileGLSL(const std::string& source,
                                              ShaderStage stage);
    VkDevice       m_Device;
    VkShaderModule m_Module = VK_NULL_HANDLE;
};

} // namespace Fufu::RHI
