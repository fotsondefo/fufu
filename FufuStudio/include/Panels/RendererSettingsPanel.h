#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>

namespace FufuStudio 
{
	class RendererSettingsPanel : public IEditorPanel
	{
	public:
		explicit RendererSettingsPanel(Fufu::Renderer& renderer)
			: m_Renderer(renderer) {}

		void onImGuiRender(EditorState& state) override;

	private:
		Fufu::Renderer& m_Renderer;
	};

}