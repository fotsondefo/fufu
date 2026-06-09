#pragma once

#include <memory>
#include "LayerStack.h"
#include "Window.h"
#include "Events/ApplicationEvents.h"

namespace Fufu 
{

	class Application
	{
	public:
		Application();
		virtual ~Application();

		void run();
		void close();

		void pushLayer(Layer* layer);
		void pushOverlay(Layer* overlay);

		static Application& get() { return *s_Instance; }
		Window& getWindow() { return *m_Window; }

	private:
		void onEvent(Event& e);
		bool onWindowClose(WindowCloseEvent& e);

	private:
		std::unique_ptr<Window> m_Window;
		LayerStack m_LayerStack;
		bool m_Running = true;
		float m_LastFrameTime = 0.0f;

		static Application* s_Instance;
	};

	Application* CreateApplication();

}