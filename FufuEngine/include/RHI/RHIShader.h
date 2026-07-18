#pragma once

#include "RHITypes.h"
#include <string>
#include <vector>

namespace Fufu::RHI
{

enum class ShaderStage { Vertex, Fragment, Compute };

struct ShaderDesc
{
    ShaderStage           stage;
    std::string           glslSource; // Backend OpenGL
    std::vector<uint8_t>  spirvCode;  // Backend Vulkan
    std::string           entryPoint = "main";
};

class RHIShader
{
public:
    virtual ~RHIShader() = default;
    ShaderStage getStage() const { return m_Desc.stage; }
protected:
    ShaderDesc m_Desc{};
};

} // namespace Fufu::RHI
