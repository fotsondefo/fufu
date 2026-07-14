#include "Layers/LabLayer.h"
#include "Demo/DemoRegistry.h"
#include <Application/Application.h>
#include <Project/ProjectManager.h>
#include <Project/Components.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace FufuLab
{
    LabLayer::LabLayer()
        : Fufu::ILayer("LabLayer")
        , m_Renderer(Fufu::Application::get().getRenderer())
    {
    }

    void LabLayer::onAttach()
    {
        initImGui();
    }

    void LabLayer::onDetach()
    {
        deactivateCurrentDemo();
        m_ActiveScene.reset();
        shutdownImGui();
    }

    void LabLayer::onUpdate(float deltaTime)
    {
        beginImGuiFrame();

        // Tant qu'aucun projet n'est chargé, on montre l'écran de sélection
        if (!m_ProjectReady)
        {
            m_ProjectReady = m_ProjectScreen.onImGuiRender();
            if (m_ProjectReady)
                onProjectLoaded();

            endImGuiFrame();
            return;
        }

        // Navigation caméra (avant le rendu pour que la position soit à jour)
        if (m_ActiveScene && m_ViewportFocused)
        {
            bool moved = m_CameraController.onUpdate(deltaTime, true);
            if (moved)
            {
                m_CameraController.syncToScene(*m_ActiveScene);
                m_Renderer.resetAccumulation();
            }
        }
        else
        {
            m_CameraController.onUpdate(deltaTime, false);
        }

        // Rendu de la scène active
        if (m_ActiveScene)
        {
            if (m_ActiveDemo)
                m_ActiveDemo->onUpdate(deltaTime);
            m_Renderer.renderScene(*m_ActiveScene);
        }

        // Layout principal
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##LabRoot", nullptr,
            ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const float leftWidth   = 220.f;
        const float rightWidth  = 270.f;
        const float statusH     = 24.f;
        const float contentH    = io.DisplaySize.y - statusH;

        ImGui::BeginChild("##Left", ImVec2(leftWidth, contentH), true);
        renderLeftPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        float vpWidth = io.DisplaySize.x - leftWidth - rightWidth
                        - ImGui::GetStyle().ItemSpacing.x * 2.f;
        ImGui::BeginChild("##Viewport", ImVec2(vpWidth, contentH), false);
        renderViewport();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##Parameters", ImVec2(rightWidth, contentH), true);
        renderParametersPanel();
        ImGui::EndChild();

        renderStatusBar();

        ImGui::End();
        endImGuiFrame();
    }

    void LabLayer::onProjectLoaded()
    {
        auto& pm = Fufu::Application::get().getProjectManager();
        if (!pm.hasProject()) return;

        auto& sm = pm.getCurrentProject().getSceneManager();

        // Activer la première scène disponible du projet
        const auto& scenes = sm.getLoadedScenes();
        if (!scenes.empty())
            activateScene(scenes.begin()->first);

        // Activer la première démo disponible
        const auto& entries = DemoRegistry::get().entries();
        if (!entries.empty())
            activateDemo(entries[0].name);

        FUFU_INFO("FufuLab: project '{}' loaded — {} scene(s)",
            pm.getCurrentProject().getName(), scenes.size());
    }

    // -------------------------------------------------------------------------
    // Panneau gauche : scènes du projet + liste des démos
    // -------------------------------------------------------------------------

    void LabLayer::renderLeftPanel()
    {
        renderSceneSelector();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        renderDemoSelector();
    }

    void LabLayer::renderSceneSelector()
    {
        auto& pm = Fufu::Application::get().getProjectManager();
        if (!pm.hasProject()) return;

        auto& sm = pm.getCurrentProject().getSceneManager();
        const auto& scenes = sm.getLoadedScenes();

        ImGui::TextDisabled("Scenes");
        ImGui::Spacing();

        if (scenes.empty())
        {
            ImGui::TextDisabled("  No scenes in project.");
            return;
        }

        for (const auto& [name, scene] : scenes)
        {
            bool active = (name == m_ActiveSceneName);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.38f, 0.60f, 1.f));

            if (ImGui::Button(name.c_str(), ImVec2(-1.f, 0.f)))
                activateScene(name);

            if (active)
                ImGui::PopStyleColor();
        }
    }

    void LabLayer::renderDemoSelector()
    {
        ImGui::TextDisabled("Demos");
        ImGui::Spacing();

        const auto& entries = DemoRegistry::get().entries();
        std::string currentCategory;

        for (const auto& entry : entries)
        {
            if (entry.category != currentCategory)
            {
                if (!currentCategory.empty()) ImGui::Spacing();
                ImGui::TextDisabled("  %s", entry.category.c_str());
                currentCategory = entry.category;
            }

            bool active = (entry.name == m_ActiveDemoName);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f));

            if (ImGui::Button(entry.name.c_str(), ImVec2(-1.f, 0.f)))
                activateDemo(entry.name);

            if (active)
                ImGui::PopStyleColor();
        }

        if (entries.empty())
        {
            ImGui::TextDisabled("  No demos registered.");
            ImGui::TextDisabled("  See FufuLab/src/demos/");
        }
    }

    // -------------------------------------------------------------------------
    // Viewport
    // -------------------------------------------------------------------------

    void LabLayer::renderViewport()
    {
        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 1.f && avail.y > 1.f)
        {
            glm::vec2 newSize = { avail.x, avail.y };
            if (newSize != m_ViewportSize)
            {
                m_ViewportSize = newSize;
                m_Renderer.resize(static_cast<int>(newSize.x), static_cast<int>(newSize.y));
            }
        }

        if (!m_ActiveScene)
        {
            ImGui::TextDisabled("Select a scene in the left panel.");
            return;
        }

        ImTextureID texID = reinterpret_cast<ImTextureID>(
            static_cast<uintptr_t>(m_Renderer.getOutputTextureID()));
        ImVec2 imagePos = ImGui::GetCursorScreenPos();
        ImGui::Image(texID, avail, ImVec2(0, 1), ImVec2(1, 0));

        if (m_ActiveDemo)
            m_ActiveDemo->onViewportOverlay(ImGui::GetWindowDrawList(), imagePos, avail);
    }

    // -------------------------------------------------------------------------
    // Panneau paramètres
    // -------------------------------------------------------------------------

    void LabLayer::renderParametersPanel()
    {
        if (!m_ActiveScene)
        {
            ImGui::TextDisabled("No scene loaded.");
            return;
        }

        if (!m_ActiveDemo)
        {
            ImGui::TextDisabled("No demo active.");
            return;
        }

        ImGui::TextUnformatted(m_ActiveDemo->getName());
        ImGui::Separator();

        const char* ref = m_ActiveDemo->getReference();
        if (ref && *ref != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.7f, 0.9f, 1.f));
            ImGui::TextWrapped("%s", ref);
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::Spacing();
        m_ActiveDemo->onParametersPanel();
    }

    // -------------------------------------------------------------------------
    // Barre de statut
    // -------------------------------------------------------------------------

    void LabLayer::renderStatusBar()
    {
        float fps = Fufu::Profiler::get().getFPS();
        float ms  = Fufu::Profiler::get().getCurrentFrame().cpuFrameTimeMs;

        auto& pm = Fufu::Application::get().getProjectManager();
        std::string projectName = pm.hasProject()
            ? pm.getCurrentProject().getName() : "No project";

        ImGui::Separator();
        ImGui::SetCursorPosX(8.f);
        ImGui::Text("%.0f FPS  |  %.2f ms  |  %s  |  %s",
            fps, ms,
            projectName.c_str(),
            m_ActiveSceneName.empty() ? "—" : m_ActiveSceneName.c_str());
    }

    // -------------------------------------------------------------------------
    // Gestion démo / scène
    // -------------------------------------------------------------------------

    void LabLayer::activateScene(const std::string& sceneName)
    {
        auto& pm = Fufu::Application::get().getProjectManager();
        if (!pm.hasProject()) return;

        auto& sm = pm.getCurrentProject().getSceneManager();
        const auto& scenes = sm.getLoadedScenes();

        auto it = scenes.find(sceneName);
        if (it == scenes.end()) return;

        // Re-attacher la démo courante à la nouvelle scène
        if (m_ActiveDemo && m_ActiveScene)
            m_ActiveDemo->onDetach();

        m_ActiveScene     = it->second;
        m_ActiveSceneName = sceneName;

        m_CameraController.syncFromScene(*m_ActiveScene);

        if (m_ActiveDemo)
            m_ActiveDemo->onAttach(*m_ActiveScene);

        m_Renderer.resetAccumulation();
        FUFU_INFO("FufuLab: active scene → '{}'", sceneName);
    }

    void LabLayer::activateDemo(const std::string& name)
    {
        if (name == m_ActiveDemoName) return;
        deactivateCurrentDemo();

        try
        {
            m_ActiveDemo     = DemoRegistry::get().create(name);
            m_ActiveDemoName = name;
            if (m_ActiveScene)
                m_ActiveDemo->onAttach(*m_ActiveScene);
            FUFU_INFO("FufuLab: active demo → '{}'", name);
        }
        catch (const std::exception& e)
        {
            FUFU_ERROR("FufuLab: failed to activate demo '{}': {}", name, e.what());
        }
    }

    void LabLayer::deactivateCurrentDemo()
    {
        if (m_ActiveDemo)
        {
            m_ActiveDemo->onDetach();
            m_ActiveDemo.reset();
            m_ActiveDemoName.clear();
        }
    }

    // -------------------------------------------------------------------------
    // ImGui lifecycle
    // -------------------------------------------------------------------------

    void LabLayer::initImGui()
    {
        void* nativeWindow = Fufu::Application::get().getWindow().getNativeWindow();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.IniFilename = nullptr;

        ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(nativeWindow), true);
        ImGui_ImplOpenGL3_Init("#version 430");

        ImGui::StyleColorsDark();
        ImGuiStyle& style    = ImGui::GetStyle();
        style.WindowRounding = 4.f;
        style.FrameRounding  = 4.f;
        style.GrabRounding   = 4.f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.f);
        style.Colors[ImGuiCol_ChildBg]  = ImVec4(0.11f, 0.12f, 0.13f, 1.f);
    }

    void LabLayer::shutdownImGui()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void LabLayer::beginImGuiFrame()
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void LabLayer::endImGuiFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
