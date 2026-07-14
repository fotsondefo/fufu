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

		// À appeler une fois juste après qu'une scène devient active (création,
		// ouverture, switch dans le ProjectPanel...). Pousse les RenderSettings
		// sauvegardés avec la scène dans le Renderer.
		// Note : la synchronisation caméra est gérée par ViewportPanel via
		// syncCameraFromScene(state), qui met à jour le CameraController interne.
		void syncToActiveScene()
		{
			auto scene = getActiveScene();
			if (!scene) return;

			Fufu::Application::get().getRenderer().getSettings() = scene->getRenderSettings();
		}
	};

}