// FUFU_VK_BACKEND défini pour ce TU → depch.h n'inclut pas glad
#define VMA_IMPLEMENTATION
#include "RHI/VK/VKResources.h"
#include "RHI/VK/VKContext.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Fufu::RHI
{

// ── Membres statiques de VKTexture ────────────────────────────────────────────

VkFormat VKTexture::toVKFormat(Format f)
{
    switch (f)
    {
    case Format::RGBA8_UNORM:      return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::RGBA16_FLOAT:     return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Format::RGBA32_FLOAT:     return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Format::R32_FLOAT:        return VK_FORMAT_R32_SFLOAT;
    case Format::DEPTH24_STENCIL8: return VK_FORMAT_D24_UNORM_S8_UINT;
    case Format::DEPTH32_FLOAT:    return VK_FORMAT_D32_SFLOAT;
    default:                        return VK_FORMAT_UNDEFINED;
    }
}

VkImageAspectFlags VKTexture::aspectOf(VkFormat fmt)
{
    if (fmt == VK_FORMAT_D24_UNORM_S8_UINT)
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    if (fmt == VK_FORMAT_D32_SFLOAT)
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

// ═══════════════════════════════════════════════════════════════════════════════
// VKBuffer
// ═══════════════════════════════════════════════════════════════════════════════

VKBuffer::VKBuffer(VkDevice device, VmaAllocator allocator, const BufferDesc& desc)
    : m_Device(device), m_Allocator(allocator)
{
    m_Desc = desc;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (hasFlag(desc.usage, BufferUsage::Vertex))
        usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (hasFlag(desc.usage, BufferUsage::Index))
        usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (hasFlag(desc.usage, BufferUsage::Uniform))
        usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (hasFlag(desc.usage, BufferUsage::Storage))
        usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (hasFlag(desc.usage, BufferUsage::TransferSrc))
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size  = desc.size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCI{};
    switch (desc.memory)
    {
    case MemoryType::GPU:
        allocCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;
    case MemoryType::CPU_TO_GPU:
        allocCI.usage = VMA_MEMORY_USAGE_AUTO;
        allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                      | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
    case MemoryType::GPU_TO_CPU:
        allocCI.usage = VMA_MEMORY_USAGE_AUTO;
        allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        break;
    }

    VK_CHECK(vmaCreateBuffer(m_Allocator, &ci, &allocCI,
                             &m_Buffer, &m_Allocation, nullptr));

    if (desc.initialData && desc.size > 0)
        upload(desc.initialData, desc.size);
}

VKBuffer::~VKBuffer()
{
    if (m_Buffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
}

void VKBuffer::upload(const void* data, uint64_t size, uint64_t offset)
{
    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(m_Allocator, m_Allocation, &mapped));
    std::memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
    vmaUnmapMemory(m_Allocator, m_Allocation);
    vmaFlushAllocation(m_Allocator, m_Allocation, offset, size);
}

void* VKBuffer::map(uint64_t /*offset*/, uint64_t /*size*/)
{
    void* ptr = nullptr;
    VK_CHECK(vmaMapMemory(m_Allocator, m_Allocation, &ptr));
    m_IsMapped = true;
    return ptr;
}

void VKBuffer::unmap()
{
    if (m_IsMapped)
    {
        vmaFlushAllocation(m_Allocator, m_Allocation, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(m_Allocator, m_Allocation);
        m_IsMapped = false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// VKTexture
// ═══════════════════════════════════════════════════════════════════════════════

VKTexture::VKTexture(VkDevice device, VmaAllocator allocator, const TextureDesc& desc)
    : m_Device(device), m_Allocator(allocator)
{
    m_Desc     = desc;
    m_VKFormat = VKTexture::toVKFormat(desc.format);
    VkImageUsageFlags usage = 0;
    auto hasTexFlag = [&](TextureUsage f) {
        return (static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(f)) != 0;
    };
    if (hasTexFlag(TextureUsage::ColorAttachment))  usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (hasTexFlag(TextureUsage::DepthAttachment))  usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (hasTexFlag(TextureUsage::Sampled))          usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (hasTexFlag(TextureUsage::StorageImage))     usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = m_VKFormat;
    ici.extent        = { desc.width, desc.height, 1 };
    ici.mipLevels     = desc.mipLevels;
    ici.arrayLayers   = (desc.type == TextureType::TextureCube) ? 6 : 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = usage;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (desc.type == TextureType::TextureCube)
        ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(m_Allocator, &ici, &allocCI,
                            &m_Image, &m_Allocation, nullptr));
    createImageView();
}

// Constructeur wrapper swapchain
VKTexture::VKTexture(VkDevice device, VkImage image, VkImageView view,
                     uint32_t width, uint32_t height, VkFormat format)
    : m_Device(device), m_Image(image), m_ImageView(view),
      m_VKFormat(format), m_IsSwapchain(true)
{
    m_Desc.width  = width;
    m_Desc.height = height;
    m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

VKTexture::~VKTexture()
{
    if (m_ImageView != VK_NULL_HANDLE)
        vkDestroyImageView(m_Device, m_ImageView, nullptr);
    if (!m_IsSwapchain && m_Image != VK_NULL_HANDLE)
        vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
}

void VKTexture::createImageView()
{
    VkImageViewCreateInfo vci{};
    vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image    = m_Image;
    vci.viewType = (m_Desc.type == TextureType::TextureCube)
                   ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    vci.format   = m_VKFormat;
    vci.subresourceRange.aspectMask = aspectOf(m_VKFormat);
    vci.subresourceRange.levelCount = m_Desc.mipLevels;
    vci.subresourceRange.layerCount =
        (m_Desc.type == TextureType::TextureCube) ? 6 : 1;
    VK_CHECK(vkCreateImageView(m_Device, &vci, nullptr, &m_ImageView));
}

void VKTexture::upload(const void* /*data*/, uint32_t /*mip*/, uint32_t /*layer*/)
{
    // TODO: staging buffer + immediate submit
    spdlog::warn("VKTexture::upload: not yet implemented (use staging path)");
}

// ═══════════════════════════════════════════════════════════════════════════════
// VKSampler
// ═══════════════════════════════════════════════════════════════════════════════

static VkFilter toVKFilter(Filter f)
{
    return (f == Filter::Nearest) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

static VkSamplerAddressMode toVKAddr(AddressMode m)
{
    switch (m)
    {
    case AddressMode::Repeat:       return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case AddressMode::ClampToEdge:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case AddressMode::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default:                         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VKSampler::VKSampler(VkDevice device, const SamplerDesc& desc)
    : m_Device(device)
{
    VkSamplerCreateInfo si{};
    si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter    = toVKFilter(desc.magFilter);
    si.minFilter    = toVKFilter(desc.minFilter);
    si.mipmapMode   = (desc.mipmapFilter == Filter::Nearest)
                      ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = toVKAddr(desc.addressU);
    si.addressModeV = toVKAddr(desc.addressV);
    si.addressModeW = toVKAddr(desc.addressW);
    si.maxLod       = VK_LOD_CLAMP_NONE;
    si.anisotropyEnable = (desc.maxAnisotropy > 1.f) ? VK_TRUE : VK_FALSE;
    si.maxAnisotropy    = desc.maxAnisotropy;
    VK_CHECK(vkCreateSampler(m_Device, &si, nullptr, &m_Sampler));
}

VKSampler::~VKSampler()
{
    if (m_Sampler != VK_NULL_HANDLE)
        vkDestroySampler(m_Device, m_Sampler, nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════════
// VKShader  —  compilation GLSL → SPIR-V via glslang
// ═══════════════════════════════════════════════════════════════════════════════

static bool s_GlslangInitialized = false;

static std::string loadVKShaderSource(const std::string& sourcePath)
{
    if (sourcePath.empty()) return {};
    // Cherche d'abord shaders/vulkan/<name>, puis shaders/<name>
    auto base = std::filesystem::current_path() / "shaders";
    std::filesystem::path vkPath = base / "vulkan" / sourcePath;
    std::filesystem::path glPath = base / sourcePath;

    auto load = [](const std::filesystem::path& p) -> std::string {
        std::ifstream f(p);
        if (!f.is_open()) return {};
        std::stringstream ss; ss << f.rdbuf(); return ss.str();
    };

    std::string src = load(vkPath);
    if (!src.empty()) return src;
    return load(glPath);
}

std::vector<uint32_t> VKShader::compileGLSL(const std::string& source,
                                              ShaderStage stage)
{
    if (!s_GlslangInitialized)
    {
        glslang::InitializeProcess();
        s_GlslangInitialized = true;
    }

    EShLanguage lang;
    switch (stage)
    {
    case ShaderStage::Vertex:   lang = EShLangVertex;   break;
    case ShaderStage::Fragment: lang = EShLangFragment; break;
    case ShaderStage::Compute:  lang = EShLangCompute;  break;
    default:                     lang = EShLangVertex;   break;
    }

    glslang::TShader shader(lang);
    const char* src = source.c_str();
    shader.setStrings(&src, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, lang,
                       glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    const TBuiltInResource* resources = GetDefaultResources();
    if (!shader.parse(resources, 450, false, EShMsgDefault))
        throw std::runtime_error(std::string("GLSL parse error:\n") +
                                 shader.getInfoLog());

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault))
        throw std::runtime_error(std::string("GLSL link error:\n") +
                                 program.getInfoLog());

    std::vector<uint32_t> spirv;
    glslang::SpvOptions opts{};
    opts.disableOptimizer = true;  // ENABLE_OPT=OFF → pas de SPIRV-Tools
    glslang::GlslangToSpv(*program.getIntermediate(lang), spirv, &opts);
    return spirv;
}

VKShader::VKShader(VkDevice device, const ShaderDesc& desc)
    : m_Device(device)
{
    m_Desc = desc;

    std::vector<uint32_t> spirv;
    if (!desc.spirvCode.empty())
    {
        // SPIR-V pré-compilé fourni directement
        spirv.resize(desc.spirvCode.size() / 4);
        std::memcpy(spirv.data(), desc.spirvCode.data(), desc.spirvCode.size());
    }
    else
    {
        // Charge la variante Vulkan depuis shaders/vulkan/ (ou compile glslSource)
        std::string src = loadVKShaderSource(desc.sourcePath);
        if (src.empty()) src = desc.glslSource;
        spirv = compileGLSL(src, desc.stage);
    }

    VkShaderModuleCreateInfo mci{};
    mci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mci.codeSize = spirv.size() * sizeof(uint32_t);
    mci.pCode    = spirv.data();
    VK_CHECK(vkCreateShaderModule(m_Device, &mci, nullptr, &m_Module));
}

VKShader::~VKShader()
{
    if (m_Module != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_Device, m_Module, nullptr);
}

} // namespace Fufu::RHI
