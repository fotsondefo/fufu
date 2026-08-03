#include "depch.h"
#include "Application/Application.h"
#include "Application/Profiler.h"
#include "Application/ImGuiSink.h"
#include "Events/ApplicationEvents.h"
#include "Project/Project.h"
#include "Project/Scene/Scene.h"
#include <GLFW/glfw3.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Fufu
{

	Application* Application::s_Instance = nullptr;

	Application::Application(const WindowProps& props)
	{
		FUFU_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		// ── Logging setup ─────────────────────────────────────────────────────
		// Build a multi-sink logger: console (colored) + rotating file + ImGui.
		auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		consoleSink->set_pattern("[%T] [%^%l%$] %v");

		auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
			"logs/fufu.log", 1024 * 1024 * 5 /*5MB*/, 3 /*keep 3 rotations*/);
		fileSink->set_pattern("[%Y-%m-%d %T] [%l] %v");

		auto imguiSink = std::make_shared<Fufu::ImGuiSink_mt>();
		imguiSink->set_pattern("[%T] [%l] %v");
		Fufu::ImGuiSink_mt::instance() = imguiSink;

		auto logger = std::make_shared<spdlog::logger>(
			"fufu", spdlog::sinks_init_list{ consoleSink, fileSink, imguiSink });
		logger->set_level(spdlog::level::trace);
		spdlog::set_default_logger(logger);
		// ──────────────────────────────────────────────────────────────────────

		m_Window = std::make_unique<Window>(props);
		m_Window->setEventCallback([this](Event& e) { onEvent(e); });

		m_Renderer.init(props.width, props.height);
		m_JobSystem.init();
		Profiler::get().init();

		// Wire ProjectManager → ModuleRegistry
		m_ProjectManager.onProjectOpened.connect([this](Project& p) {
			m_ModuleRegistry.notifyProjectOpened(p);
			// Wire the SceneManager of the newly opened project
			p.getSceneManager().onSceneActivated.connect(
				[this](Scene& s) { m_ModuleRegistry.notifySceneActivated(s); });
			p.getSceneManager().onSceneClosed.connect(
				[this]() { m_ModuleRegistry.notifySceneClosed(); });
		});
		m_ProjectManager.onProjectClosed.connect([this]() {
			m_ModuleRegistry.notifyProjectClosed();
			// SceneManager signals are destroyed with the project
		});

		std::filesystem::path appConfig = std::filesystem::current_path() / "config";
		m_ProjectManager.init(appConfig);
	}

	Application::~Application()
	{
		m_ModuleRegistry.deactivateAll();

		// Join all background threads BEFORE closing the project (which
		// destroys assets): prevents a still-running job from touching a
		// MeshAsset/TextureAsset while it is being freed.
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

			// Apply the results of background jobs completed since the
			// previous frame (GPU upload, state mutation...) — see
			// JobSystem: this is the only place these callbacks execute.
			m_JobSystem.pollMainThreadCallbacks();

			Profiler::get().beginFrame();

			if (!m_Minimized)
			{
				m_ModuleRegistry.tickAll(static_cast<double>(deltaTime));

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