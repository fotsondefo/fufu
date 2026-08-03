#include "Layers/RuntimeLayer.h"
#include "Application/Application.h"
#include "Project/Scene/SceneSerializer.h"
#include "Project/Components.h"
#include "Events/KeyEvents.h"

namespace FufuRuntime 
{

	void RuntimeLayer::onAttach()
	{
		auto& app = Fufu::Application::get();
		auto& renderer = app.getRenderer();

		// Apply renderer settings from the config file
		renderer.getSettings().maxBounces = m_Config.maxBounces;
		renderer.getSettings().samplesPerPixel = m_Config.samplesPerPixel;
		renderer.getSettings().exposure = m_Config.exposure;
		renderer.getSettings().mode = Fufu::RenderMode::Accumulation;

		// Resolve the scene path
		// First we check relatively to the .exe dir, otherwise we take the absolute path
		std::filesystem::path exeDir = std::filesystem::path(app.getWindow().getNativeWindow() ? std::filesystem::current_path() : ".");

		std::filesystem::path scenePath = m_Config.scenePath;
		if (!scenePath.is_absolute())
			scenePath = exeDir / scenePath;

		if (!std::filesystem::exists(scenePath))
		{
			FUFU_ERROR("Scene file not found: '{}'", scenePath.string());
			app.close();
			return;
		}

		// Load the scene
		m_Scene = std::make_shared<Fufu::Scene>();
		Fufu::SceneSerializer serializer(m_Scene.get());

		if (!serializer.deserialize(scenePath))
		{
			FUFU_ERROR("Failed to deserialize scene: '{}'", scenePath.string());
			app.close();
			return;
		}

		renderer.resetAccumulation();
		FUFU_INFO("Runtime scene loaded: '{}'", scenePath.string());
	}

	void RuntimeLayer::onDetach() {}

	void RuntimeLayer::onUpdate(float /*deltaTime*/)
	{
		if (!m_Scene) return;

		Fufu::Application::get().getRenderer().renderScene(*m_Scene);

		// Direct blit to the framebuffer by default
		// The renderer has already written to the output texture
		// Window::onUpdate() calls glfwSwapBuffers
	}

	void RuntimeLayer::onEvent(Fufu::Event& e)
	{
		Fufu::EventDispatcher dispatcher(e);

		dispatcher.dispatch<Fufu::KeyPressedEvent>(
			[this](Fufu::KeyPressedEvent& ev) 
			{
				return onKeyPressed(ev); 
			}
		);
	}

	bool RuntimeLayer::onKeyPressed(Fufu::KeyPressedEvent& e)
	{
		if (e.getKeyCode() == GLFW_KEY_ESCAPE)
		{
			Fufu::Application::get().close();
			return true;
		}
		return false;
	}

}