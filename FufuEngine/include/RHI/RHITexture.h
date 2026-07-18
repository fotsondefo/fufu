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

    // Upload depuis la mémoire CPU. VK : staging buffer + pipeline barrier interne.
    virtual void upload(const void* data, uint32_t mip = 0, uint32_t layer = 0) = 0;

    // Identifiant natif du backend : GLuint en GL, VkImage en VK.
    // Permet à du code GL-specifique de récupérer le handle sans inclure GLResources.h.
    virtual uint64_t getNativeHandle() const = 0;

    const TextureDesc& getDesc() const { return m_Desc; }

protected:
    TextureDesc m_Desc{};
};

} // namespace Fufu::RHI
