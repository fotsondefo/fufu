#pragma once

#include "RHITypes.h"
#include "RHITexture.h"

namespace Fufu::RHI
{

struct SwapchainDesc
{
    void*    windowHandle;            // GLFWwindow* (GL) or HWND/surface (VK)
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

    // Returns the target texture for the current frame.
    // GL: nullptr → the CommandList binds FBO 0 (default back buffer).
    virtual RHITexture* acquireNextImage() = 0;

    // GL: glfwSwapBuffers   VK: vkQueuePresentKHR
    virtual void present() = 0;

    virtual uint32_t getWidth()  const = 0;
    virtual uint32_t getHeight() const = 0;
};

} // namespace Fufu::RHI
