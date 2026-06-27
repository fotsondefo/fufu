#pragma once

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <Project/Entity.h>
#include <Project/Scene.h>
#include "Panels/ImGuiContext.h"

namespace FufuStudio 
{
	struct EditorState
	{
		// Scène active
		std::shared_ptr<Fufu::Scene> activeScene;

		// Entité sélectionnée dans la hiérarchie
		Fufu::Entity selectedEntity;

		std::string currentPath;

		// Caméra éditeur
		glm::vec3 cameraPosition = { 0.f, 1.f, 5.f };
		glm::vec3 cameraRotation = { 0.f, 0.f, 0.f }; // pitch, yaw, roll en radians
		float     cameraMoveSpeed = 5.f;
		float     cameraLookSpeed = 0.1f;

		// Viewport
		glm::vec2 viewportSize = { 1280.f, 720.f };
		bool      viewportFocused = false;
		bool      viewportHovered = false;
		ImGuiContext* imGuiContext = nullptr;
	};

}