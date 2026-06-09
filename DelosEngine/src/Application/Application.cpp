#include "depch.h"
#include "Delos/Application.h"
#include <GLFW/glfw3.h>

namespace Delos {

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		DELOS_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::make_unique<Window>(WindowProps{ "Delos", 1280, 720 });
		m_Window->setEventCallback([this](Event& e) { onEvent(e); });
	}

	Application::~Application() = default;

	void Application::run()
	{
		while (m_Running)
		{
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - m_LastFrameTime;
			m_LastFrameTime = time;

			// Mise à jour des layers du bas vers le haut
			for (Layer* layer : m_LayerStack)
				layer->onUpdate(deltaTime);

			m_Window->onUpdate();
		}
	}

	void Application::close()
	{
		m_Running = false;
	}

	void Application::pushLayer(Layer* layer)
	{
		m_LayerStack.pushLayer(layer);
	}

	void Application::pushOverlay(Layer* overlay)
	{
		m_LayerStack.pushOverlay(overlay);
	}

	void Application::onEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.dispatch<WindowCloseEvent>(
			[this](WindowCloseEvent& e) { return onWindowClose(e); }
		);

		// Propagation aux layers du haut vers le bas, s'arrête si l'event est consommé
		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			--it;
			(*it)->onEvent(e);
			if (e.handled)
				break;
		}
	}

	bool Application::onWindowClose(WindowCloseEvent&)
	{
		close();
		return true;
	}

}