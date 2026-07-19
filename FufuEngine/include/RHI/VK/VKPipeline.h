#pragma once

#include "VKCommon.h"
#include "RHI/RHIPipeline.h"

namespace Fufu::RHI
{

class VKPipeline final : public RHIPipeline
{
public:
    VKPipeline(VkDevice device, VkPipelineLayout layout,
               const GraphicsPipelineDesc& desc);
    VKPipeline(VkDevice device, VkPipelineLayout layout,
               const ComputePipelineDesc& desc);
    ~VKPipeline() override;

    VkPipeline getPipeline() const { return m_Pipeline; }

private:
    static VkShaderStageFlagBits toVKStage(ShaderStage s);
    static VkCompareOp           toVKCompare(CompareOp op);
    static VkBlendOp             toVKBlendOp(BlendOp op);
    static VkBlendFactor         toVKBlendFactor(BlendFactor f);
    static VkFormat              toVKFormat(Format f);

    VkDevice   m_Device   = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
};

} // namespace Fufu::RHI
