#pragma once

// Inclus par les TUs VK (FUFU_VK_BACKEND défini → glad exclu de depch.h).
// Ce header apporte Vulkan + VMA + GLFW Vulkan + helpers.

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace Fufu::RHI
{

// ── Macro de vérification VK ──────────────────────────────────────────────────
#define VK_CHECK(call)                                                          \
    do {                                                                        \
        VkResult _vkr = (call);                                                 \
        if (_vkr != VK_SUCCESS)                                                 \
            throw std::runtime_error(std::string("Vulkan error ")               \
                + std::to_string(static_cast<int>(_vkr))                       \
                + " at " __FILE__ ":" + std::to_string(__LINE__));              \
    } while (0)

// ── Constantes de layout descriptor ─────────────────────────────────────────
// UBO  slots : binding  0-15  → RHI slot 0-15
// SSBO slots : binding 16-31  → RHI slot  + 16
// Sampler    : binding 32-63  → RHI slot  + 32
constexpr uint32_t kDescUBOBase     =  0u;
constexpr uint32_t kDescSSBOBase    = 16u;
constexpr uint32_t kDescSamplerBase = 32u;
constexpr uint32_t kDescTotalSlots  = 64u;

constexpr uint32_t kFramesInFlight  =  2u;

} // namespace Fufu::RHI
