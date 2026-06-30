#include "depch.h"
#include "Application/Window.h"
#include "Events/ApplicationEvents.h"
#include "Events/KeyEvents.h"
#include "Events/MouseEvents.h"

namespace Fufu 
{

	static uint8_t s_GLFWWindowCount = 0;

	static void glfwErrorCallback(int error, const char* description)
	{
		FUFU_ERROR("GLFW error #{}: {}", error, description);
	}

	Window::Window(const WindowProps& props)
	{
		init(props);
	}

	Window::~Window()
	{
		shutdown();
	}

	void Window::init(const WindowProps& props)
	{
		m_Data.title = props.title;
		m_Data.width = props.width;
		m_Data.height = props.height;

		if (s_GLFWWindowCount == 0)
		{
			int success = glfwInit();
			FUFU_ASSERT(success, "Failed to initialize GLFW");
			glfwSetErrorCallback(glfwErrorCallback);
		}

		FUFU_INFO("Creating window '{}' ({}x{})", props.title, props.width, props.height);

		glfwWindowHint(GLFW_TITLEBAR, GLFW_FALSE);

		m_Window = glfwCreateWindow(
			static_cast<int>(props.width),
			static_cast<int>(props.height),
			props.title.c_str(),
			nullptr, nullptr
		);
		++s_GLFWWindowCount;

		glfwMakeContextCurrent(m_Window);

		int gladStatus = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		FUFU_ASSERT(gladStatus, "Failed to initialize GLAD");

		glfwSetWindowUserPointer(m_Window, &m_Data);
		setVSync(true);

		// --- Callbacks GLFW -> Events Fufu ---

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* w, int width, int height)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(w));
			data.width = width;
			data.height = height;
			WindowResizeEvent event(width, height);
			data.eventCallback(event);
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* w)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(w));
			WindowCloseEvent event;
			data.eventCallback(event);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(w));
			switch (action)
			{
			case GLFW_PRESS:
			{
				KeyPressedEvent event(key, false);
				data.eventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent event(key);
				data.eventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent event(key, true);
				data.eventCallback(event);
				break;
			}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* w, unsigned int codepoint)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(w));
			KeyTypedEvent event(static_cast<int>(codepoint));
			data.eventCallback(event);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* w, int button, int action, int /*mods*/)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(w));
			if (action == GLFW_PRESS)
			{
				MouseButtonPressedEvent event(button);
				data.eventCallback(event);
			}
			else
			{
				MouseButtonReleasedEvent event(button);
				data.eventCallback(event);
			}
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* w, double xOffset, double yOffset)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(w));
			MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
			data.eventCallback(event);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* w, double x, double y)
		{
			auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(w));
			MouseMovedEvent event(static_cast<float>(x), static_cast<float>(y));
			data.eventCallback(event);
		});
	}

	void Window::shutdown()
	{
		glfwDestroyWindow(m_Window);
		--s_GLFWWindowCount;

		if (s_GLFWWindowCount == 0)
			glfwTerminate();
	}

	void Window::onUpdate()
	{
		glfwPollEvents();
		glfwSwapBuffers(m_Window);
	}

	void Window::setVSync(bool enabled)
	{
		glfwSwapInterval(enabled ? 1 : 0);
		m_Data.vSync = enabled;
	}

}