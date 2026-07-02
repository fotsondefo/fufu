#pragma once

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <Project/Entity.h>
#include <Project/Scene/Scene.h>
#include <Application/Application.h>
#include "Panels/ImGuiContext.h"
#include "Selection.h"

namespace FufuStudio
{
	class CommandHistory;

	struct EditorState
	{
		Selection selection;

		// Camera
		glm::vec3 cameraPosition = { 0.f, 1.f, 5.f };
		glm::vec3 cameraRotation = { 0.f, 0.f, 0.f }; // pitch, yaw, roll en radians
		float     cameraMoveSpeed = 5.f;
		float     cameraLookSpeed = 0.1f;

		// Viewport
		// viewportPos = coin haut-gauche de l'IMAGE rendue en coordonnées écran
		// (pas la fenêtre ImGui du panneau, qui inclut sa propre barre de titre).
		// Les outils (gizmo, pick de face...) doivent l'utiliser pour que leur
		// projection reste alignée avec ce qui est réellement affiché.
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
	};

}