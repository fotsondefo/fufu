#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>
#include <Application/CameraController.h>
#include "Tools/ToolManager.h"
#include "Helpers/OrientationGizmo.h"
#include <string>

namespace FufuStudio
{

	class ViewportPanel : public IEditorPanel
	{
	public:
		explicit ViewportPanel(Fufu::Renderer& renderer);

		void onImGuiRender(EditorState& state) override;
		void onUpdate(EditorState& state, float deltaTime);

		// Call when the active scene changes (open, switch...)
		// to initialise the camera from the existing entity.
		void syncCameraFromScene(EditorState& state);

	private:
		void drawContextMenu(EditorState& state);
		void drawImportModal(EditorState& state);
		void exportRenderedImage();

		// Enqueues an entity create from a file path (accepts DND_FILEPATH drops).
		// Opens the import-scale modal first; the entity is spawned on OK.
		void beginMeshImportFromPath(const std::string& path);

	private:
		Fufu::Renderer&       m_Renderer;
		Fufu::CameraController m_CameraController;
		ToolManager            m_ToolManager;
		OrientationGizmo       m_OrientationGizmo;

		bool m_OpenContextMenu = false;

		// Detect scene change for automatic camera resync
		Fufu::Scene* m_LastScene = nullptr;

		// Import-scale modal state
		struct PendingImport {
			std::string path;
			uint64_t    assetID  = 0;
			float       scale    = 1.f;
			bool        open     = false; // triggers ImGui::OpenPopup on the next frame
		};
		PendingImport m_PendingImport;
	};

}
