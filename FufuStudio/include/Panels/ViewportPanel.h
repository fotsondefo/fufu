#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>
#include <Application/CameraController.h>
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

		// À appeler quand la scène active change (ouverture, switch...)
		// pour initialiser la caméra depuis l'entité existante.
		void syncCameraFromScene(EditorState& state);

	private:
		void drawContextMenu(EditorState& state);
		void exportRenderedImage();

	private:
		Fufu::Renderer&       m_Renderer;
		Fufu::CameraController m_CameraController;
		ToolManager            m_ToolManager;
		OrientationGizmo       m_OrientationGizmo;

		bool m_OpenContextMenu = false;

		// Détection de changement de scène pour resync automatique de la caméra
		Fufu::Scene* m_LastScene = nullptr;
	};

}
