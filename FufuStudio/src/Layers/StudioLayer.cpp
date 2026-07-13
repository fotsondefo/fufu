#include "Layers/StudioLayer.h"
#include <Application/Application.h>
#include <Project/Components.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include "SceneIO.h"

namespace FufuStudio 
{

	StudioLayer::StudioLayer()
		: Fufu::ILayer("StudioLayer")
		, m_ViewportPanel(Fufu::Application::get().getRenderer())
		, m_RendererSettingsPanel(Fufu::Application::get().getRenderer())
		, m_ProfilerPanel(Fufu::Application::get().getRenderer())
	{
		m_State.commandHistory = &m_CommandHistory;
	}

	void StudioLayer::onAttach()
	{
		auto* nativeWindow = Fufu::Application::get().getWindow().getNativeWindow();
		std::filesystem::path configDir = std::filesystem::current_path() / "config" / "editor";

		m_ImGuiContext.init(nativeWindow, configDir);

		m_State.imGuiContext = &m_ImGuiContext;

		m_TitleBar.init(configDir / "logo.png");
	}

	void StudioLayer::onDetach()
	{
		m_TitleBar.shutdown();
		m_ImGuiContext.shutdown();
	}

	void StudioLayer::onUpdate(float deltaTime)
	{
		handleKeyboardShortcuts();

		m_ImGuiContext.beginFrame();

		m_TitleBar.onImGuiRender(m_State);

		// Welcome screen until a project is onpen
		if (!m_ProjectReady)
		{
			m_ProjectReady = m_WelcomeScreen.onImGuiRender(m_State);
			m_ImGuiContext.endFrame();
			return;
		}

		// Sync camera + rendu
		auto scene = m_State.getActiveScene();
		if (scene)
		{
			m_ViewportPanel.onUpdate(m_State, deltaTime);
			Fufu::Application::get().getRenderer().renderScene(*scene);
		}

		buildDockspace();

		m_ViewportPanel.onImGuiRender(m_State);
		m_RendererSettingsPanel.onImGuiRender(m_State);
		m_HierarchyPanel.onImGuiRender(m_State);
		m_InspectorPanel.onImGuiRender(m_State);
		m_ProjectPanel.onImGuiRender(m_State);
		m_ProfilerPanel.onImGuiRender(m_State);

		m_ImGuiContext.endFrame();
	}

	void StudioLayer::onEvent(Fufu::Event& /*e*/)
	{
		// Les events souris/clavier sont lus directement via GLFW dans ViewportPanel
		// quand le viewport est focused � pas besoin de dispatcher ici pour l'instant
	}

	void StudioLayer::buildDockspace()
	{
		// No Docking
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowPos(ImVec2(
			viewport->Pos.x,
			viewport->Pos.y + TitleBar::k_Height
		)); 
		ImGui::SetNextWindowSize(ImVec2(
			viewport->Size.x,
			viewport->Size.y - TitleBar::k_Height
		));

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoBackground;

		ImGui::Begin("MainWindow", nullptr, flags);

		ImGui::End();
	}


	void StudioLayer::handleKeyboardShortcuts()
	{
		ImGuiIO& io = ImGui::GetIO();

		if (io.KeyCtrl)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_S))
			{
				if (io.KeyShift)
					SceneIO::saveSceneAs(m_State);
				else
					SceneIO::saveScene(m_State);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_O))
				SceneIO::openScene(m_State);

			if (ImGui::IsKeyPressed(ImGuiKey_N))
				SceneIO::newScene(m_State);

			// Ne pas intercepter Ctrl+Z/Y pendant qu'un widget (ex : InputText)
			// a le focus : il g�re son propre undo local.
			if (!ImGui::IsAnyItemActive())
			{
				if (ImGui::IsKeyPressed(ImGuiKey_Z))
					m_CommandHistory.undo();
				if (ImGui::IsKeyPressed(ImGuiKey_Y))
					m_CommandHistory.redo();
			}
		}
	}

}