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
	{}

	void StudioLayer::onAttach()
	{
		auto* nativeWindow = Fufu::Application::get().getWindow().getNativeWindow();
		std::filesystem::path configDir = std::filesystem::current_path() / "config" / "editor";

		m_ImGuiContext.init(nativeWindow, configDir);

		m_State.activeScene = std::make_shared<Fufu::Scene>("Default Scene");
		m_State.imGuiContext = &m_ImGuiContext;

		// Main Camera
		auto cam = m_State.activeScene->createEntity("Editor Camera");
		auto& camComp = cam.addComponent<Fufu::CameraComponent>();
		camComp.primary = true;
		camComp.fov = glm::radians(45.f);

		auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
		camTransform.position = m_State.cameraPosition;

		auto sphere = m_State.activeScene->createEntity("Test Sphere");
		auto& mesh = sphere.addComponent<Fufu::MeshComponent>();
		mesh.meshPath = "assets/meshes/sphere.obj";

		auto& mat = sphere.addComponent<Fufu::MaterialComponent>();
		mat.albedo = glm::vec4(0.8f, 0.2f, 0.2f, 1.f);
		mat.roughness = 0.4f;
		mat.metallic = 0.f;

		Fufu::Application::get().getRenderer().resetAccumulation();
	}

	void StudioLayer::onDetach()
	{
		m_ImGuiContext.shutdown();
	}

	void StudioLayer::onUpdate(float deltaTime)
	{
		handleKeyboardShortcuts();

		m_ViewportPanel.onUpdate(m_State, deltaTime);

		if (m_State.activeScene)
			Fufu::Application::get().getRenderer().renderScene(*m_State.activeScene);

		m_ImGuiContext.beginFrame();
		buildDockspace();

		m_ViewportPanel.onImGuiRender(m_State);
		m_RendererSettingsPanel.onImGuiRender(m_State);
		m_HierarchyPanel.onImGuiRender(m_State); 
		m_InspectorPanel.onImGuiRender(m_State);   

		m_ImGuiContext.endFrame();
	}

	void StudioLayer::onEvent(Fufu::Event& /*e*/)
	{
		// Les events souris/clavier sont lus directement via GLFW dans ViewportPanel
		// quand le viewport est focused — pas besoin de dispatcher ici pour l'instant
	}

	void StudioLayer::buildDockspace()
	{
		// Version sans docking - fenêtre plein écran simple
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_MenuBar |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoBackground;

		ImGui::Begin("MainWindow", nullptr, flags);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
					SceneIO::newScene(m_State);

				if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
					SceneIO::openScene(m_State);

				ImGui::Separator();

				if (ImGui::MenuItem("Save", "Ctrl+S"))
					SceneIO::saveScene(m_State);

				if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
					SceneIO::saveSceneAs(m_State);

				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
					Fufu::Application::get().close();

				ImGui::EndMenu();
			}

			// Afficher le nom de la scène courante dans la barre
			if (m_State.activeScene)
			{
				std::string title = m_State.activeScene->getName();
				if (!m_State.currentPath.empty())
					title += " — " + std::filesystem::path(
						m_State.currentPath).filename().string();
				else
					title += " *";

				ImGui::SetCursorPosX(
					(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(
						title.c_str()).x) * 0.5f
				);
				ImGui::TextDisabled("%s", title.c_str());
			}

			ImGui::EndMenuBar();
		}

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
		}
	}

}