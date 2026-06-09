#pragma once

#include "Events/Event.h"
#include <functional>
#include <GLFW/glfw3.h>


namespace Fufu 
{

	struct WindowProps
	{
		std::string  title;
		unsigned int width;
		unsigned int height;

		explicit WindowProps(
			const std::string& title = "Fufu Engine",
			unsigned int width = 1280,
			unsigned int  height = 720)
			: title(title), width(width), height(height) {}
	};

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		explicit Window(const WindowProps& props);
		~Window();

		void onUpdate();

		unsigned int getWidth()  const { return m_Data.width; }
		unsigned int getHeight() const { return m_Data.height; }
		void* getNativeWindow()  const { return m_Window; }

		void setEventCallback(const EventCallbackFn& callback) { m_Data.eventCallback = callback; }
		void setVSync(bool enabled);
		bool isVSync() const { return m_Data.vSync; }

	private:
		void init(const WindowProps& props);
		void shutdown();

	private:
		GLFWwindow* m_Window = nullptr;

		// All data passed to GLFW callbacks via glfwSetWindowUserPointer
		struct WindowData
		{
			std::string title;
			unsigned int width = 0;
			unsigned int height = 0;
			bool vSync = true;
			EventCallbackFn eventCallback;
		};

		WindowData m_Data;
	};

}