#pragma once

#include <Application/ILayer.h>
#include "EditorState.h"
#include "Panels/ViewportPanel.h"
#include "Panels/RendererSettingsPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ImGuiContext.h"
#include "Panels/ProjectPanel.h"
#include "Screens/WelcomeScreen.h"
#include "Helpers/TitleBar.h"

namespace FufuStudio 
{

	class StudioLayer : public Fufu::ILayer
	{
	public:
		StudioLayer();

		void onAttach() override;
		void onDetach() override;
		void onUpdate(float deltaTime) override;
		void onEvent(Fufu::Event& e) override;

	private:
		void buildDockspace();
		void handleKeyboardShortcuts();

	private:
		ImGuiContext m_ImGuiContext;
		EditorState m_State;
		ViewportPanel m_ViewportPanel;
		RendererSettingsPanel m_RendererSettingsPanel;
		HierarchyPanel  m_HierarchyPanel;
		InspectorPanel  m_InspectorPanel;
		ProjectPanel m_ProjectPanel;
		
		WelcomeScreen m_WelcomeScreen;
		TitleBar m_TitleBar;

		bool m_ProjectReady = false;
	};

} 