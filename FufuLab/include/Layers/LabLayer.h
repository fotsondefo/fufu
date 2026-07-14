#pragma once

#include <Application/ILayer.h>
#include <Renderer/Renderer.h>
#include <Project/Scene/Scene.h>
#include <Application/Profiler.h>
#include "Demo/IDemo.h"
#include "Screens/LabProjectScreen.h"
#include <memory>
#include <string>

namespace FufuLab
{
    class LabLayer : public Fufu::ILayer
    {
    public:
        LabLayer();

        void onAttach() override;
        void onDetach() override;
        void onUpdate(float deltaTime) override;

    private:
        void initImGui();
        void shutdownImGui();
        void beginImGuiFrame();
        void endImGuiFrame();

        // Appelé une seule fois après le chargement du projet
        void onProjectLoaded();

        void renderLeftPanel();
        void renderSceneSelector();
        void renderDemoSelector();
        void renderViewport();
        void renderParametersPanel();
        void renderStatusBar();

        void activateDemo(const std::string& name);
        void deactivateCurrentDemo();
        void activateScene(const std::string& sceneName);

        Fufu::Renderer& m_Renderer;

        // Project screen
        LabProjectScreen m_ProjectScreen;
        bool             m_ProjectReady = false;

        // Scène active (tirée du projet chargé)
        std::shared_ptr<Fufu::Scene> m_ActiveScene;
        std::string                  m_ActiveSceneName;

        // Démo active
        std::unique_ptr<IDemo> m_ActiveDemo;
        std::string            m_ActiveDemoName;

        // Viewport
        glm::vec2 m_ViewportSize = { 0.f, 0.f };
    };
}
