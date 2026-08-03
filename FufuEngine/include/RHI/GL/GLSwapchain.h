#pragma once

#include "RHI/RHISwapchain.h"

struct GLFWwindow;

namespace Fufu::RHI
{

class GLSwapchain final : public RHISwapchain
{
public:
    explicit GLSwapchain(const SwapchainDesc& desc);
    ~GLSwapchain() override = default;

    void resize(uint32_t width, uint32_t height) override;

    // GL has no distinct swapchain texture — returns nullptr.
    // The CommandList will interpret nullptr as FBO 0 (back buffer).
    RHITexture* acquireNextImage() override { return nullptr; }

    void present() override; // glfwSwapBuffers

    uint32_t getWidth()  const override { return m_Width;  }
    uint32_t getHeight() const override { return m_Height; }

private:
    GLFWwindow* m_Window = nullptr;
    uint32_t    m_Width  = 0;
    uint32_t    m_Height = 0;
};

} // namespace Fufu::RHI
