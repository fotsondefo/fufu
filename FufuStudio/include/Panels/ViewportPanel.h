#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>
#include "Tools/ToolManager.h"

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

	private:
		Fufu::Renderer& m_Renderer;
		ToolManager m_ToolManager;

		// Tracking souris pour le mode FPS
		glm::vec2 m_LastMousePos = { 0.f, 0.f };
		bool      m_FirstMouse = true;
	};

}
