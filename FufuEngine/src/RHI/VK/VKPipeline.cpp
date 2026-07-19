#include "RHI/VK/VKPipeline.h"
#include "RHI/VK/VKResources.h"
#include <stdexcept>

namespace Fufu::RHI
{

// ── Conversions statiques ─────────────────────────────────────────────────────

VkShaderStageFlagBits VKPipeline::toVKStage(ShaderStage s)
{
    switch (s)
    {
    case ShaderStage::Vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderStage::Compute:  return VK_SHADER_STAGE_COMPUTE_BIT;
    default:                     return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

VkCompareOp VKPipeline::toVKCompare(CompareOp op)
{
    switch (op)
    {
    case CompareOp::Never:        return VK_COMPARE_OP_NEVER;
    case CompareOp::Less:         return VK_COMPARE_OP_LESS;
    case CompareOp::Equal:        return VK_COMPARE_OP_EQUAL;
    case CompareOp::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater:      return VK_COMPARE_OP_GREATER;
    case CompareOp::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::Always:       return VK_COMPARE_OP_ALWAYS;
    default:                       return VK_COMPARE_OP_LESS;
    }
}

VkBlendOp VKPipeline::toVKBlendOp(BlendOp op)
{
    switch (op)
    {
    case BlendOp::Add:      return VK_BLEND_OP_ADD;
    case BlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
    case BlendOp::Min:      return VK_BLEND_OP_MIN;
    case BlendOp::Max:      return VK_BLEND_OP_MAX;
    default:                 return VK_BLEND_OP_ADD;
    }
}

VkBlendFactor VKPipeline::toVKBlendFactor(BlendFactor f)
{
    switch (f)
    {
    case BlendFactor::Zero:              return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:               return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcAlpha:          return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha:          return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:  return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:                              return VK_BLEND_FACTOR_ONE;
    }
}

VkFormat VKPipeline::toVKFormat(Format f)
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

// ── Pipeline graphique ────────────────────────────────────────────────────────

VKPipeline::VKPipeline(VkDevice device, VkPipelineLayout layout,
                       const GraphicsPipelineDesc& desc)
    : m_Device(device)
{
    m_IsCompute = false;

    auto* vs = static_cast<VKShader*>(desc.vertexShader.get());
    auto* fs = static_cast<VKShader*>(desc.fragmentShader.get());

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs->getModule();
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs->getModule();
    stages[1].pName  = "main";

    // Pas de vertex buffer — les shaders fetchent les sommets via SSBOs
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport & scissor dynamiques (définis via vkCmdSetViewport/Scissor)
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rast{};
    rast.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = (desc.rasterizer.fillMode == FillMode::Wireframe)
                       ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rast.cullMode    = (desc.rasterizer.cullMode == CullMode::None)   ? VK_CULL_MODE_NONE :
                       (desc.rasterizer.cullMode == CullMode::Front)  ? VK_CULL_MODE_FRONT_BIT :
                                                                         VK_CULL_MODE_BACK_BIT;
    rast.frontFace   = (desc.rasterizer.frontFace == FrontFace::CCW)
                       ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rast.lineWidth   = 1.0f;
    rast.depthBiasEnable         = (desc.rasterizer.depthBiasConstant != 0.f ||
                                    desc.rasterizer.depthBiasSlope    != 0.f);
    rast.depthBiasConstantFactor = desc.rasterizer.depthBiasConstant;
    rast.depthBiasSlopeFactor    = desc.rasterizer.depthBiasSlope;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = desc.depthStencil.depthTestEnable  ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = desc.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp   = toVKCompare(desc.depthStencil.depthCompareOp);

    const auto& ba = desc.blendAttachment;
    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable         = ba.blendEnable ? VK_TRUE : VK_FALSE;
    blendAtt.srcColorBlendFactor = toVKBlendFactor(ba.srcColor);
    blendAtt.dstColorBlendFactor = toVKBlendFactor(ba.dstColor);
    blendAtt.colorBlendOp        = toVKBlendOp(ba.colorOp);
    blendAtt.srcAlphaBlendFactor = toVKBlendFactor(ba.srcAlpha);
    blendAtt.dstAlphaBlendFactor = toVKBlendFactor(ba.dstAlpha);
    blendAtt.alphaBlendOp        = toVKBlendOp(ba.alphaOp);
    blendAtt.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // Plusieurs color attachments peuvent être présents (ex: G-Buffer: 3)
    std::vector<VkPipelineColorBlendAttachmentState> blendAtts(
        std::max(1u, (uint32_t)desc.colorFormats.size()), blendAtt);

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = (uint32_t)blendAtts.size();
    blend.pAttachments    = blendAtts.data();

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    // Dynamic rendering — pas de VkRenderPass
    std::vector<VkFormat> colorFmts;
    colorFmts.reserve(desc.colorFormats.size());
    for (auto f : desc.colorFormats) colorFmts.push_back(toVKFormat(f));

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount    = (uint32_t)colorFmts.size();
    renderingInfo.pColorAttachmentFormats = colorFmts.data();
    renderingInfo.depthAttachmentFormat   = toVKFormat(desc.depthFormat);

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.pNext               = &renderingInfo;
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rast;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &blend;
    pci.pDynamicState       = &dyn;
    pci.layout              = layout;
    pci.renderPass          = VK_NULL_HANDLE;

    VK_CHECK(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pci, nullptr, &m_Pipeline));
}

// ── Pipeline compute ──────────────────────────────────────────────────────────

VKPipeline::VKPipeline(VkDevice device, VkPipelineLayout layout,
                       const ComputePipelineDesc& desc)
    : m_Device(device)
{
    m_IsCompute = true;
    auto* cs = static_cast<VKShader*>(desc.computeShader.get());

    VkComputePipelineCreateInfo pci{};
    pci.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pci.stage.module = cs->getModule();
    pci.stage.pName  = "main";
    pci.layout       = layout;

    VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &pci, nullptr, &m_Pipeline));
}

VKPipeline::~VKPipeline()
{
    if (m_Pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
}

} // namespace Fufu::RHI
