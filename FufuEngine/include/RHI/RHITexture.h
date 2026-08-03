#pragma once

#include "RHITypes.h"

namespace Fufu::RHI
{

enum class TextureType { Texture2D, TextureCube };

struct TextureDesc
{
    TextureType  type      = TextureType::Texture2D;
    Format       format    = Format::RGBA8_UNORM;
    uint32_t     width     = 1;
    uint32_t     height    = 1;
    uint32_t     mipLevels = 1;
    TextureUsage usage     = TextureUsage::Sampled;
};

class RHITexture
{
public:
    virtual ~RHITexture() = default;

    // Upload from CPU memory. VK: staging buffer + internal pipeline barrier.
    virtual void upload(const void* data, uint32_t mip = 0, uint32_t layer = 0) = 0;

    // Native backend identifier: GLuint in GL, VkImage in VK.
    // Allows GL-specific code to retrieve the handle without including GLResources.h.
    virtual uint64_t getNativeHandle() const = 0;

    const TextureDesc& getDesc() const { return m_Desc; }

protected:
    TextureDesc m_Desc{};
};

} // namespace Fufu::RHI
