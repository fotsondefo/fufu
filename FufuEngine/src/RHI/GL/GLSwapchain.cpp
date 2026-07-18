#include "depch.h"
#include "RHI/GL/GLSwapchain.h"
#include <GLFW/glfw3.h>

namespace Fufu::RHI
{

GLSwapchain::GLSwapchain(const SwapchainDesc& desc)
    : m_Window(static_cast<GLFWwindow*>(desc.windowHandle))
    , m_Width(desc.width)
    , m_Height(desc.height)
{
    if (desc.vsync)
        glfwSwapInterval(1);
    else
        glfwSwapInterval(0);
}

void GLSwapchain::resize(uint32_t width, uint32_t height)
{
    m_Width  = width;
    m_Height = height;
    // Le viewport est mis à jour dans beginRenderPass ou par l'application.
}

void GLSwapchain::present()
{
    glfwSwapBuffers(m_Window);
}

} // namespace Fufu::RHI
