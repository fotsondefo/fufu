#include "depch.h"
#include "Application/Application.h"
#include "Application/Profiler.h"
#include "Events/ApplicationEvents.h"
#include <GLFW/glfw3.h>

namespace Fufu
{

	Application* Application::s_Instance = nullptr;

	Application::Application(const WindowProps& props)
	{
		FUFU_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::make_unique<Window>(props);
		m_Window->setEventCallback([this](Event& e) { onEvent(e); });

		m_Renderer.init(props.width, props.height);
		m_JobSystem.init();
		Profiler::get().init();

		std::filesystem::path appConfig = std::filesystem::current_path() / "config";
		m_ProjectManager.init(appConfig);
	}

	Application::~Application()
	{
		// Joint tous les threads de fond AVANT de fermer le projet (qui
		// détruit les assets) : évite qu'un job encore en cours touche un
		// MeshAsset/TextureAsset pendant qu'il est libéré.
		m_JobSystem.shutdown();

		Profiler::get().shutdown();
		m_Renderer.shutdown();
		m_ProjectManager.shutdown();
	}

	void Application::run()
	{
		while (m_Running)
		{
			float time = static_cast<float>(glfwGetTime());
			float deltaTime = time - m_LastFrameTime;
			m_LastFrameTime = time;

			// Applique les résultats des jobs de fond terminés depuis la
			// frame précédente (upload GPU, mutation d'état...) — voir
			// JobSystem : c'est le seul endroit où ces callbacks s'exécutent.
			m_JobSystem.pollMainThreadCallbacks();

			Profiler::get().beginFrame();

			if (!m_Minimized)
			{
				for (ILayer* layer : m_LayerStack)
					layer->onUpdate(deltaTime);
			}

			m_Window->onUpdate();

			Profiler::get().endFrame();
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
		dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& ev) { return onWindowClose(ev); });
		dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& ev) { return onWindowResize(ev); });

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