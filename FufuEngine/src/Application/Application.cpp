#include "depch.h"
#include "Application/Application.h"
#include "Events/ApplicationEvents.h"
#include <GLFW/glfw3.h>

namespace Fufu
{

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		FUFU_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::make_unique<Window>(WindowProps{ "Fufu Engine", 1920, 1080 });
		m_Window->setEventCallback([this](Event& e) { onEvent(e); });

		m_Renderer.init(1280, 720);
	}

	Application::~Application()
	{
		m_Renderer.shutdown();
	}

	void Application::run()
	{
		while (m_Running)
		{
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - m_LastFrameTime;
			m_LastFrameTime = time;

			if (!m_Minimized)
			{
				for (ILayer* layer : m_LayerStack)
					layer->onUpdate(deltaTime);
			}

			m_Window->onUpdate();
		}
	}

	void Application::close() 
	{ 
		m_Running = false; 
	}

	void Application::pushLayer(ILayer* layer) 
	{ 
		m_LayerStack.pushLayer(layer); 
	}

	void Application::onEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.dispatch<WindowCloseEvent>(
			[this](WindowCloseEvent& ev) { return onWindowClose(ev); });
		dispatcher.dispatch<WindowResizeEvent>(
			[this](WindowResizeEvent& ev) { return onWindowResize(ev); });

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			--it;
			(*it)->onEvent(e);
			if (e.handled) break;
		}
	}

	bool Application::onWindowClose(WindowCloseEvent&)
	{
		close();
		return true;
	}

	bool Application::onWindowResize(WindowResizeEvent& e)
	{
		if (e.getWidth() == 0 || e.getHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}
		m_Minimized = false;
		m_Renderer.resize(e.getWidth(), e.getHeight());
		return false;
	}

}