#pragma once

#include "RHITypes.h"
#include "RHITexture.h"

namespace Fufu::RHI
{

struct SwapchainDesc
{
    void*    windowHandle;            // GLFWwindow* (GL) ou HWND/surface (VK)
    uint32_t width;
    uint32_t height;
    uint32_t imageCount = 2;          // double / triple buffering
    bool     vsync      = true;
    Format   format     = Format::RGBA8_UNORM;
};

class RHISwapchain
{
public:
    virtual ~RHISwapchain() = default;

    virtual void resize(uint32_t width, uint32_t height) = 0;

    // Retourne la texture cible de la frame courante.
    // GL : nullptr → le CommandList bind FBO 0 (back buffer par défaut).
    virtual RHITexture* acquireNextImage() = 0;

    // GL : glfwSwapBuffers   VK : vkQueuePresentKHR
    virtual void present() = 0;

    virtual uint32_t getWidth()  const = 0;
    virtual uint32_t getHeight() const = 0;
};

} // namespace Fufu::RHI
