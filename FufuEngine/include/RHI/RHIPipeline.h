#pragma once

#include "RHITypes.h"
#include "RHIShader.h"
#include <vector>

namespace Fufu::RHI
{

// ── Rasterizer ───────────────────────────────────────────────────────────────
enum class FillMode  { Solid, Wireframe };
enum class CullMode  { None, Front, Back };
enum class FrontFace { CCW, CW };

struct RasterizerState
{
    FillMode  fillMode         = FillMode::Solid;
    CullMode  cullMode         = CullMode::Back;
    FrontFace frontFace        = FrontFace::CCW;
    float     depthBiasConstant = 0.f;
    float     depthBiasSlope    = 0.f;
};

// ── Depth-stencil ────────────────────────────────────────────────────────────
enum class CompareOp
{
    Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always
};

struct DepthStencilState
{
    bool      depthTestEnable  = true;
    bool      depthWriteEnable = true;
    CompareOp depthCompareOp   = CompareOp::Less;
};

// ── Blend ────────────────────────────────────────────────────────────────────
enum class BlendFactor
{
    Zero, One,
    SrcAlpha, OneMinusSrcAlpha,
    DstAlpha, OneMinusDstAlpha,
};
enum class BlendOp { Add, Subtract, Min, Max };

struct BlendAttachment
{
    bool        blendEnable = false;
    BlendFactor srcColor    = BlendFactor::SrcAlpha;
    BlendFactor dstColor    = BlendFactor::OneMinusSrcAlpha;
    BlendOp     colorOp     = BlendOp::Add;
    BlendFactor srcAlpha    = BlendFactor::One;
    BlendFactor dstAlpha    = BlendFactor::Zero;
    BlendOp     alphaOp     = BlendOp::Add;
};

// ── Descripteurs ─────────────────────────────────────────────────────────────
struct GraphicsPipelineDesc
{
    Ref<RHIShader>    vertexShader;
    Ref<RHIShader>    fragmentShader;
    RasterizerState   rasterizer;
    DepthStencilState depthStencil;
    BlendAttachment   blendAttachment;
    // Formats des attachments — nécessaires pour VK (compat render pass)
    std::vector<Format> colorFormats;
    Format              depthFormat = Format::DEPTH24_STENCIL8;
};

struct ComputePipelineDesc
{
    Ref<RHIShader> computeShader;
};

// ── Interface ────────────────────────────────────────────────────────────────
class RHIPipeline
{
public:
    virtual ~RHIPipeline() = default;
    bool isCompute() const { return m_IsCompute; }
protected:
    bool m_IsCompute = false;
};

} // namespace Fufu::RHI
