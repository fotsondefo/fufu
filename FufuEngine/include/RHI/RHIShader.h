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
    std::string           glslSource;             // OpenGL backend
    std::string           sourcePath;             // File name (e.g. "GBuffer.vert")
    std::vector<uint8_t>  spirvCode;              // Vulkan backend (pre-compiled)
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
