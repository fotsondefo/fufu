#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>
#include <Project/Components.h>
#include <optional>

namespace FufuStudio
{

	class ViewportPanel : public IEditorPanel
	{
	public:
		explicit ViewportPanel(Fufu::Renderer& renderer)
			: m_Renderer(renderer) {}

		void onImGuiRender(EditorState& state) override;
		void onUpdate(EditorState& state, float deltaTime);

	private:
		void handleCameraInput(EditorState& state, float deltaTime);
		void syncCameraToScene(EditorState& state);
		void drawGizmo(EditorState& state);
		void handleGizmoShortcuts(EditorState& state);

	private:
		Fufu::Renderer& m_Renderer;

		// Tracking souris pour le mode FPS
		glm::vec2 m_LastMousePos = { 0.f, 0.f };
		bool      m_FirstMouse = true;

		// Tracking du drag de gizmo pour l'undo : snapshot pris juste avant
		// le d�but du drag, commande pouss�e quand il se termine.
		bool m_GizmoWasUsing = false;
		std::optional<Fufu::TransformComponent> m_GizmoBeforeEdit;
	};

}