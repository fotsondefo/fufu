#include "CornellBoxDemo.h"
#include "Demo/DemoRegistry.h"
#include <Application/Application.h>
#include <Renderer/Renderer.h>
#include <Application/Profiler.h>
#include <imgui.h>

FUFULAB_REGISTER_DEMO("Rendering", "Cornell Box", FufuLab::CornellBoxDemo);

namespace FufuLab
{
    void CornellBoxDemo::onAttach(Fufu::Scene& scene)
    {
        (void)scene;
        m_Renderer = &Fufu::Application::get().getRenderer();
        m_Renderer->resetAccumulation();
    }

    void CornellBoxDemo::onDetach()
    {
        m_Renderer = nullptr;
    }

    void CornellBoxDemo::onParametersPanel()
    {
        if (!m_Renderer) return;

        auto& settings = m_Renderer->getSettings();
        bool changed = false;

        // --- Path tracer ---
        ImGui::TextDisabled("Path Tracer");
        ImGui::Spacing();

        int bounces = settings.maxBounces;
        if (ImGui::SliderInt("Max Bounces", &bounces, 1, 16))
        {
            settings.maxBounces = bounces;
            changed = true;
        }

        int spp = settings.samplesPerPixel;
        if (ImGui::SliderInt("Samples / Pixel", &spp, 1, 8))
        {
            settings.samplesPerPixel = spp;
            changed = true;
        }

        float exposure = settings.exposure;
        if (ImGui::SliderFloat("Exposure", &exposure, 0.01f, 10.f, "%.2f"))
        {
            settings.exposure = exposure;
            changed = true;
        }

        ImGui::Spacing();

        // --- Mode ---
        ImGui::TextDisabled("Mode");
        ImGui::Spacing();

        const char* techniques[] = { "Path Tracing", "Ray Tracing (Whitted)" };
        int tech = static_cast<int>(settings.technique);
        if (ImGui::Combo("Technique", &tech, techniques, 2))
        {
            settings.technique = static_cast<Fufu::RenderTechnique>(tech);
            changed = true;
        }

        bool realtime = (settings.mode == Fufu::RenderMode::Realtime);
        if (ImGui::Checkbox("Realtime (no accumulation)", &realtime))
        {
            settings.mode = realtime ? Fufu::RenderMode::Realtime : Fufu::RenderMode::Accumulation;
            changed = true;
        }

        if (changed)
            m_Renderer->resetAccumulation();

        // --- Stats ---
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Statistics");
        ImGui::Spacing();

        const auto& frame = Fufu::Profiler::get().getCurrentFrame();
        float fps = Fufu::Profiler::get().getFPS();

        ImGui::Text("FPS          %.0f", fps);
        ImGui::Text("CPU frame    %.2f ms", frame.cpuFrameTimeMs);
        if (frame.gpuSectionsMs.count("ComputePass"))
            ImGui::Text("GPU compute  %.2f ms", frame.gpuSectionsMs.at("ComputePass"));
        ImGui::Text("Triangles    %d", frame.triangleCount);
        ImGui::Text("Instances    %d", frame.instanceCount);
        ImGui::Text("Accumulated  %d samples", m_Renderer->getAccumulatedFrames());
    }
}
