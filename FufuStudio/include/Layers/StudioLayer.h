#pragma once

#include <Application/Layer.h>
#include "EditorState.h"
#include "Panels/ViewportPanel.h"
#include "Panels/RendererSettingsPanel.h"

namespace FufuStudio 
{

	class StudioLayer : public Fufu::Layer
	{
	public:
		StudioLayer();

		void onAttach() override;
		void onDetach() override;
		void onUpdate(float deltaTime) override;
		void onEvent(Fufu::Event& e) override;

	private:
		void initImGui();
		void shutdownImGui();
		void beginImGuiFrame();
		void endImGuiFrame();
		void buildDockspace();

	private:
		EditorState            m_State;
		ViewportPanel          m_ViewportPanel;
		RendererSettingsPanel  m_RendererSettingsPanel;
	};

} 