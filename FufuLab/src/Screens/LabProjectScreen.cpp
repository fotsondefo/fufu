#include "Screens/LabProjectScreen.h"
#include <Application/Application.h>
#include <Project/ProjectManager.h>
#include <imgui.h>
#include <nfd.hpp>

namespace FufuLab
{
    bool LabProjectScreen::onImGuiRender()
    {
        auto& pm = Fufu::Application::get().getProjectManager();
        if (pm.hasProject()) return true;

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(560.f, 420.f), ImGuiCond_Always);

        ImGui::Begin("##LabWelcome",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse);

        // En-tête
        ImGui::Spacing();
        ImGui::SetCursorPosX(20.f);
        ImGui::Text("FufuLab");
        ImGui::SameLine();
        ImGui::TextDisabled("— Research & Experimentation");
        ImGui::Spacing();
        ImGui::TextDisabled("  Open a FufuStudio project to access its scenes and assets.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Projets récents
        ImGui::TextDisabled("Recent projects");
        ImGui::Spacing();
        drawRecentProjects();

        // Bouton open en bas
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 52.f);
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Open Project...", ImVec2(-1.f, 32.f)))
        {
            if (openProjectDialog())
            {
                ImGui::End();
                return true;
            }
        }

        ImGui::End();
        return false;
    }

    void LabProjectScreen::drawRecentProjects()
    {
        auto& pm = Fufu::Application::get().getProjectManager();
        const auto& recents = pm.getRecentProjects();

        if (recents.isEmpty())
        {
            ImGui::SetCursorPosX(20.f);
            ImGui::TextDisabled("No recent projects.");
            return;
        }

        ImGui::BeginChild("##Recents", ImVec2(0.f, -64.f), false);

        for (const auto& entry : recents.getEntries())
        {
            ImGui::PushID(entry.path.string().c_str());

            bool clicked = ImGui::Selectable("##row", false,
                ImGuiSelectableFlags_None, ImVec2(0.f, 44.f));

            ImVec2 rowMin = ImGui::GetItemRectMin();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(ImVec2(rowMin.x + 8.f, rowMin.y + 6.f),
                IM_COL32(220, 220, 220, 255), entry.name.c_str());
            dl->AddText(ImVec2(rowMin.x + 8.f, rowMin.y + 24.f),
                IM_COL32(120, 120, 120, 255), entry.path.string().c_str());

            ImGui::Separator();
            ImGui::PopID();

            if (clicked && pm.openProject(entry.path))
            {
                ImGui::EndChild();
                return;
            }
        }

        ImGui::EndChild();
    }

    bool LabProjectScreen::openProjectDialog()
    {
        NFD::Guard guard;
        NFD::UniquePath path;
        nfdfilteritem_t filter = { "Fufu Project", "fufuproj" };

        if (NFD::OpenDialog(path, &filter, 1) == NFD_OKAY)
        {
            auto& pm = Fufu::Application::get().getProjectManager();
            return pm.openProject(std::filesystem::path(path.get())) != nullptr;
        }
        return false;
    }
}
