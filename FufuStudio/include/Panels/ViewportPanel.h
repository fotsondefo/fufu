#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>
#include "Tools/ToolManager.h"
#include "Helpers/OrientationGizmo.h"

namespace FufuStudio
{

	class ViewportPanel : public IEditorPanel
	{
	public:
		explicit ViewportPanel(Fufu::Renderer& renderer);

		void onImGuiRender(EditorState& state) override;
		void onUpdate(EditorState& state, float deltaTime);

	private:
		void handleCameraInput(EditorState& state, float deltaTime);
		void syncCameraToScene(EditorState& state);
		void drawContextMenu(EditorState& state);
		void exportRenderedImage();

	private:
		Fufu::Renderer& m_Renderer;
		ToolManager m_ToolManager;
		OrientationGizmo m_OrientationGizmo;

		// Tracking souris pour le mode FPS
		glm::vec2 m_LastMousePos = { 0.f, 0.f };
		bool      m_FirstMouse = true;

		// Le clic droit sert déjà à la rotation caméra (FPS look) : on ne veut
		// ouvrir le menu contextuel que sur un clic droit "propre" (pas de
		// déplacement significatif de la souris pendant qu'il était enfoncé),
		// pas après un drag de rotation — d'où ce tracking manuel plutôt que
		// le ImGui::BeginPopupContextWindow habituel (qui s'ouvrirait aussi
		// après un drag).
		float m_RightClickDragDistance = 0.f;
		bool  m_RightMouseWasDown = false;
		bool  m_OpenContextMenu = false;
	};

}
