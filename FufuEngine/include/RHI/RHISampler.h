#pragma once

#include "RHITypes.h"

namespace Fufu::RHI
{

enum class Filter      { Nearest, Linear };
enum class AddressMode { Repeat, ClampToEdge, MirrorRepeat };

struct SamplerDesc
{
    Filter      minFilter     = Filter::Linear;
    Filter      magFilter     = Filter::Linear;
    Filter      mipmapFilter  = Filter::Linear;
    AddressMode addressU      = AddressMode::Repeat;
    AddressMode addressV      = AddressMode::Repeat;
    AddressMode addressW      = AddressMode::Repeat;
    float       maxAnisotropy = 1.0f;
};

class RHISampler
{
public:
    virtual ~RHISampler() = default;
    const SamplerDesc& getDesc() const { return m_Desc; }
protected:
    SamplerDesc m_Desc{};
};

} // namespace Fufu::RHI
