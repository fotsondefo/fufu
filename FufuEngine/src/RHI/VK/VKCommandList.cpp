#include "RHI/VK/VKCommandList.h"
#include "RHI/VK/VKResources.h"
#include "RHI/VK/VKPipeline.h"
#include <stdexcept>
#include <cassert>

namespace Fufu::RHI
{

static VkImageLayout toVKLayout(TextureLayout l)
{
    switch (l)
    {
    case TextureLayout::Undefined:         return VK_IMAGE_LAYOUT_UNDEFINED;
    case TextureLayout::ColorAttachment:   return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case TextureLayout::DepthAttachment:   return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case TextureLayout::ShaderReadOnly:    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case TextureLayout::StorageImage:      return VK_IMAGE_LAYOUT_GENERAL;
    case TextureLayout::Present:           return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case TextureLayout::TransferSrc:       return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case TextureLayout::TransferDst:       return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:                                return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

// ── Init ──────────────────────────────────────────────────────────────────────

void VKCommandList::begin(VkCommandBuffer cmd, VkDescriptorSet descSet,
                          VkDescriptorSetLayout layout, VkPipelineLayout pipelineLayout,
                          VkDevice device)
{
    m_CommandBuffer  = cmd;
    m_DescriptorSet  = descSet;
    m_Layout         = layout;
    m_PipelineLayout = pipelineLayout;
    m_Device         = device;
    m_DescDirty      = false;
    m_InsidePass     = false;
    m_PendingWrites.clear();
    m_BufferInfos.clear();
    m_ImageInfos.clear();
}

void VKCommandList::end()
{
    if (m_InsidePass) endRenderPass();
    VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));
}

// ── Descriptors ───────────────────────────────────────────────────────────────

void VKCommandList::updateDescriptor(uint32_t binding, VkDescriptorType type,
                                     VkBuffer buffer, VkDeviceSize offset,
                                     VkDeviceSize range,
                                     VkImageView imageView,
                                     VkSampler sampler,
                                     VkImageLayout layout)
{
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = m_DescriptorSet;
    write.dstBinding      = binding;
    write.descriptorCount = 1;
    write.descriptorType  = type;

    if (buffer != VK_NULL_HANDLE)
    {
        m_BufferInfos.push_back({ buffer, offset, range });
        write.pBufferInfo = &m_BufferInfos.back();
    }
    else
    {
        m_ImageInfos.push_back({ sampler, imageView, layout });
        write.pImageInfo = &m_ImageInfos.back();
    }

    m_PendingWrites.push_back(write);
    m_DescDirty = true;
}

void VKCommandList::flushDescriptors()
{
    if (!m_DescDirty || m_PendingWrites.empty()) return;

    vkUpdateDescriptorSets(m_Device,
                           (uint32_t)m_PendingWrites.size(),
                           m_PendingWrites.data(), 0, nullptr);
    m_PendingWrites.clear();
    m_BufferInfos.clear();
    m_ImageInfos.clear();

    vkCmdBindDescriptorSets(m_CommandBuffer,
                            m_InsidePass ? VK_PIPELINE_BIND_POINT_GRAPHICS
                                         : VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    m_DescDirty = false;
}

// ── Render pass (dynamic rendering) ──────────────────────────────────────────

void VKCommandList::beginRenderPass(const RenderPassDesc& desc)
{
    std::vector<VkRenderingAttachmentInfo> colorAtts;
    colorAtts.reserve(desc.colorAttachments.size());

    for (auto& ca : desc.colorAttachments)
    {
        auto* tex = static_cast<VKTexture*>(ca.texture.get());

        // Transition vers COLOR_ATTACHMENT_OPTIMAL si besoin
        if (tex->getCurrentLayout() != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            VkImageMemoryBarrier2 bar{};
            bar.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            bar.srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            bar.srcAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            bar.dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            bar.dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            bar.oldLayout           = tex->getCurrentLayout();
            bar.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            bar.image               = tex->getImage();
            bar.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            VkDependencyInfo dep{};
            dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep.imageMemoryBarrierCount = 1;
            dep.pImageMemoryBarriers    = &bar;
            vkCmdPipelineBarrier2(m_CommandBuffer, &dep);
            tex->setCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        VkRenderingAttachmentInfo att{};
        att.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        att.imageView   = tex->getImageView();
        att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.loadOp      = (ca.loadOp  == LoadOp::Clear)    ? VK_ATTACHMENT_LOAD_OP_CLEAR  :
                          (ca.loadOp  == LoadOp::Load)     ? VK_ATTACHMENT_LOAD_OP_LOAD   :
                                                              VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.storeOp     = (ca.storeOp == StoreOp::Store)   ? VK_ATTACHMENT_STORE_OP_STORE :
                                                              VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.clearValue.color = { ca.clearColor[0], ca.clearColor[1],
                                 ca.clearColor[2], ca.clearColor[3] };
        colorAtts.push_back(att);
    }

    VkRenderingAttachmentInfo depthAtt{};
    bool hasDepth = desc.depthAttachment.has_value();
    if (hasDepth)
    {
        auto* dtex = static_cast<VKTexture*>(desc.depthAttachment->texture.get());

        if (dtex->getCurrentLayout() != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            VkImageMemoryBarrier2 bar{};
            bar.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            bar.srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            bar.srcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            bar.dstStageMask     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            bar.dstAccessMask    = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            bar.oldLayout        = dtex->getCurrentLayout();
            bar.newLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            bar.image            = dtex->getImage();
            bar.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                     0, 1, 0, 1 };

            VkDependencyInfo dep{};
            dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep.imageMemoryBarrierCount = 1;
            dep.pImageMemoryBarriers    = &bar;
            vkCmdPipelineBarrier2(m_CommandBuffer, &dep);
            dtex->setCurrentLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        depthAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAtt.imageView   = dtex->getImageView();
        depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAtt.loadOp      = (desc.depthAttachment->loadOp == LoadOp::Clear)
                               ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAtt.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.clearValue.depthStencil = { desc.depthAttachment->clearDepth, 0 };
    }

    VkRenderingInfo ri{};
    ri.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea           = { {0, 0}, { desc.width, desc.height } };
    ri.layerCount           = 1;
    ri.colorAttachmentCount = (uint32_t)colorAtts.size();
    ri.pColorAttachments    = colorAtts.data();
    ri.pDepthAttachment     = hasDepth ? &depthAtt : nullptr;

    vkCmdBeginRendering(m_CommandBuffer, &ri);
    m_InsidePass = true;
}

void VKCommandList::endRenderPass()
{
    vkCmdEndRendering(m_CommandBuffer);
    m_InsidePass = false;
}

// ── État ──────────────────────────────────────────────────────────────────────

void VKCommandList::setViewport(float x, float y, float w, float h,
                                float minDepth, float maxDepth)
{
    VkViewport vp{ x, y + h, w, -h, minDepth, maxDepth }; // flip Y pour Vulkan
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &vp);
}

void VKCommandList::setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    VkRect2D sc{ {x, y}, {w, h} };
    vkCmdSetScissor(m_CommandBuffer, 0, 1, &sc);
}

void VKCommandList::bindPipeline(RHIPipeline* pipeline)
{
    auto* vkp = static_cast<VKPipeline*>(pipeline);
    VkPipelineBindPoint bp = vkp->isCompute()
                             ? VK_PIPELINE_BIND_POINT_COMPUTE
                             : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(m_CommandBuffer, bp, vkp->getPipeline());

    // Re-bind descriptor set after pipeline bind
    vkCmdBindDescriptorSets(m_CommandBuffer, bp, m_PipelineLayout,
                             0, 1, &m_DescriptorSet, 0, nullptr);
}

// ── Binding ressources ────────────────────────────────────────────────────────

void VKCommandList::bindUniformBuffer(uint32_t slot, RHIBuffer* buffer,
                                      uint64_t offset, uint64_t size)
{
    auto* vkb = static_cast<VKBuffer*>(buffer);
    VkDeviceSize sz = (size == WHOLE_SIZE) ? VK_WHOLE_SIZE : (VkDeviceSize)size;
    updateDescriptor(kDescUBOBase + slot, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     vkb->getBuffer(), offset, sz);
}

void VKCommandList::bindStorageBuffer(uint32_t slot, RHIBuffer* buffer,
                                      uint64_t offset, uint64_t size)
{
    auto* vkb = static_cast<VKBuffer*>(buffer);
    VkDeviceSize sz = (size == WHOLE_SIZE) ? VK_WHOLE_SIZE : (VkDeviceSize)size;
    updateDescriptor(kDescSSBOBase + slot, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     vkb->getBuffer(), offset, sz);
}

void VKCommandList::bindTexture(uint32_t slot, RHITexture* texture, RHISampler* sampler)
{
    auto* vkt = static_cast<VKTexture*>(texture);
    VkSampler vkSam = VK_NULL_HANDLE;
    if (sampler) vkSam = static_cast<VKSampler*>(sampler)->getSampler();

    updateDescriptor(kDescSamplerBase + slot, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     VK_NULL_HANDLE, 0, 0,
                     vkt->getImageView(), vkSam,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VKCommandList::bindStorageImage(uint32_t slot, RHITexture* texture)
{
    auto* vkt = static_cast<VKTexture*>(texture);
    updateDescriptor(kDescSamplerBase + slot, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     VK_NULL_HANDLE, 0, 0,
                     vkt->getImageView(), VK_NULL_HANDLE,
                     VK_IMAGE_LAYOUT_GENERAL);
}

// ── Draw / Dispatch ───────────────────────────────────────────────────────────

void VKCommandList::draw(uint32_t vertexCount, uint32_t instanceCount,
                         uint32_t firstVertex, uint32_t firstInstance)
{
    flushDescriptors();
    vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VKCommandList::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                uint32_t firstIndex, int32_t vertexOffset,
                                uint32_t firstInstance)
{
    flushDescriptors();
    vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount,
                     firstIndex, vertexOffset, firstInstance);
}

void VKCommandList::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
    flushDescriptors();
    vkCmdDispatch(m_CommandBuffer, groupsX, groupsY, groupsZ);
}

// ── Barrières ─────────────────────────────────────────────────────────────────

void VKCommandList::barrier(const TextureBarrier& b)
{
    auto* vkt = static_cast<VKTexture*>(b.texture.get());
    VkImageLayout from = toVKLayout(b.from);
    VkImageLayout to   = toVKLayout(b.to);

    bool isDepth = (vkt->getVKFormat() == VK_FORMAT_D24_UNORM_S8_UINT ||
                    vkt->getVKFormat() == VK_FORMAT_D32_SFLOAT);
    VkImageAspectFlags aspect = isDepth
        ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
        : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier2 bar{};
    bar.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    bar.srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    bar.srcAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    bar.dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    bar.dstAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    bar.oldLayout        = from;
    bar.newLayout        = to;
    bar.image            = vkt->getImage();
    bar.subresourceRange = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

    VkDependencyInfo dep{};
    dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers    = &bar;
    vkCmdPipelineBarrier2(m_CommandBuffer, &dep);
    vkt->setCurrentLayout(to);
}

// ── Debug labels ──────────────────────────────────────────────────────────────

void VKCommandList::beginLabel(const char* label, float r, float g, float b)
{
    VkDebugUtilsLabelEXT lbl{};
    lbl.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    lbl.pLabelName = label;
    lbl.color[0]   = r; lbl.color[1] = g; lbl.color[2] = b; lbl.color[3] = 1.f;

    auto fn = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(m_Device, "vkCmdBeginDebugUtilsLabelEXT"));
    if (fn) fn(m_CommandBuffer, &lbl);
}

void VKCommandList::endLabel()
{
    auto fn = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(m_Device, "vkCmdEndDebugUtilsLabelEXT"));
    if (fn) fn(m_CommandBuffer);
}

} // namespace Fufu::RHI
