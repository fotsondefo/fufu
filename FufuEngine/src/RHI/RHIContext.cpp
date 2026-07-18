#include "RHI/RHIContext.h"
#include "RHI/GL/GLContext.h"

namespace Fufu::RHI
{

Ref<RHIContext> RHIContext::create(const ContextDesc& desc)
{
    switch (desc.backend)
    {
    case Backend::OpenGL:
        return std::make_shared<GLContext>(desc);
    case Backend::Vulkan:
        // VKContext à implémenter
        throw std::runtime_error("Vulkan backend not yet implemented");
    }
    return nullptr;
}

} // namespace Fufu::RHI
