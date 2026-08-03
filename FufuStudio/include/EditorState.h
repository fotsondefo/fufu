#pragma once

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <Project/Entity.h>
#include <Project/Components.h>
#include <Project/Scene/Scene.h>
#include <Application/Application.h>
#include <Renderer/Renderer.h>
#include "Panels/ImGuiContext.h"
#include "Selection.h"

namespace FufuStudio
{
	class CommandHistory;

	struct EditorState
	{
		Selection selection;

		// Viewport
		// viewportPos = top-left corner of the rendered IMAGE in screen coordinates
		// (not the ImGui panel window, which includes its own title bar).
		// Tools (gizmo, face pick...) must use this so their projection stays
		// aligned with what is actually displayed.
		glm::vec2 viewportPos = { 0.f, 0.f };
		glm::vec2 viewportSize = { 1280.f, 720.f };
		bool      viewportFocused = false;
		bool      viewportHovered = false;

		// IMGUI
		ImGuiContext* imGuiContext = nullptr;

		// Undo/Redo
		CommandHistory* commandHistory = nullptr;

		std::shared_ptr<Fufu::Scene> getActiveScene() const
		{
			auto& pm = Fufu::Application::get().getProjectManager();
		
			if (!pm.hasProject()) 
				return nullptr;

			return pm.getCurrentProject().getSceneManager().getActiveScene();
		}

		bool hasProject() const
		{
			return Fufu::Application::get().getProjectManager().hasProject();
		}

		// Call once right after a scene becomes active (creation, opening,
		// switch in ProjectPanel...). Pushes the RenderSettings saved with
		// the scene into the Renderer.
		// Note: camera sync is handled by ViewportPanel via
		// syncCameraFromScene(state), which updates the internal CameraController.
		void syncToActiveScene()
		{
			auto scene = getActiveScene();
			if (!scene) return;

			Fufu::Application::get().getRenderer().getSettings() = scene->getRenderSettings();
		}
	};

}