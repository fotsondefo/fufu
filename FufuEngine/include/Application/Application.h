#pragma once

#include <memory>
#include "LayerStack.h"
#include "Window.h"
#include "Events/ApplicationEvents.h"
#include "Renderer/Renderer.h"
#include "Project/ProjectManager.h"
#include "Application/JobSystem.h"

namespace Fufu 
{

	class Application
	{
	public:
		explicit Application(const WindowProps& props = WindowProps{ "Fufu Engine", 1280, 720 });
		virtual ~Application();

		void run();
		void close();

		void pushLayer(ILayer* layer);

		static Application& get() { return *s_Instance; }
		Window& getWindow() { return *m_Window; }
		Renderer& getRenderer() { return m_Renderer; }
		ProjectManager& getProjectManager() { return m_ProjectManager; }
		JobSystem& getJobSystem() { return m_JobSystem; }

	private:
		void onEvent(Event& e);
		bool onWindowClose(WindowCloseEvent& e);
		bool onWindowResize(WindowResizeEvent& e);

	private:
		std::unique_ptr<Window> m_Window;
		LayerStack m_LayerStack;
		Renderer m_Renderer;
		ProjectManager m_ProjectManager;
		JobSystem m_JobSystem;

		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;

		static Application* s_Instance;
	};

	Application* CreateApplication();
}
