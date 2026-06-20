#include "Layers/StudioLayer.h"
#include <Application/Application.h>
#include <Project/Components.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace FufuStudio 
{

	StudioLayer::StudioLayer()
		: Fufu::Layer("StudioLayer")
		, m_ViewportPanel(Fufu::Application::get().getRenderer())
		, m_RendererSettingsPanel(Fufu::Application::get().getRenderer())
	{}

	void StudioLayer::onAttach()
	{
		initImGui();

		m_State.activeScene = std::make_shared<Fufu::Scene>("Default Scene");

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
		shutdownImGui();
	}

	void StudioLayer::onUpdate(float deltaTime)
	{
		m_ViewportPanel.onUpdate(m_State, deltaTime);

		if (m_State.activeScene)
			Fufu::Application::get().getRenderer().renderScene(*m_State.activeScene);

		beginImGuiFrame();
		buildDockspace();

		m_ViewportPanel.onImGuiRender(m_State);
		m_RendererSettingsPanel.onImGuiRender(m_State);
		m_HierarchyPanel.onImGuiRender(m_State); 
		m_InspectorPanel.onImGuiRender(m_State);   

		endImGuiFrame();
	}

	void StudioLayer::onEvent(Fufu::Event& /*e*/)
	{
		// Les events souris/clavier sont lus directement via GLFW dans ViewportPanel
		// quand le viewport est focused — pas besoin de dispatcher ici pour l'instant
	}

	// ----------------------------------------------------------------
	// ImGui init / shutdown
	// ----------------------------------------------------------------

	void StudioLayer::initImGui()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui::StyleColorsDark();

		// Arrondir légèrement les coins
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 4.f;
		style.FrameRounding = 3.f;
		style.GrabRounding = 3.f;

		GLFWwindow* window = static_cast<GLFWwindow*>(
			Fufu::Application::get().getWindow().getNativeWindow());

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 430");
	}

	void StudioLayer::shutdownImGui()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void StudioLayer::beginImGuiFrame()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void StudioLayer::endImGuiFrame()
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoBackground;

		ImGui::Begin("MainWindow", nullptr, flags);
		ImGui::End();
	}

}