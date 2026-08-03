#include "RHI/RHIContext.h"
#include "RHI/GL/GLContext.h"

#ifdef FUFU_BACKEND_VULKAN
#  include "RHI/VK/VKContext.h"
#endif

namespace Fufu::RHI
{

Ref<RHIContext> RHIContext::create(const ContextDesc& desc)
{
    switch (desc.backend)
    {
    case Backend::OpenGL:
        return std::make_shared<GLContext>(desc);
    case Backend::Vulkan:
#ifdef FUFU_BACKEND_VULKAN
        return std::make_shared<VKContext>(desc);
#else
        throw std::runtime_error("Recompile avec FUFU_BACKEND_VULKAN=ON pour Vulkan");
#endif
    }
    return nullptr;
}

} // namespace Fufu::RHI
