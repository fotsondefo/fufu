#include "Screens/WelcomeScreen.h"
#include "Application/Application.h"
#include "Helpers/FontIcons.h"
#include <imgui.h>
#include <nfd.hpp>

namespace FufuStudio 
{

	bool WelcomeScreen::onImGuiRender(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (pm.hasProject()) return true;

		ImGuiViewport* vp = ImGui::GetMainViewport();
		
		ImGui::SetNextWindowPos(
			ImVec2(vp->Pos.x + vp->Size.x * 0.5f,
				vp->Pos.y + vp->Size.y * 0.5f),
			ImGuiCond_Always, ImVec2(0.5f, 0.5f)
		);

		ImGui::SetNextWindowSize(ImVec2(600.f, 480.f), ImGuiCond_Always);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse;

		ImGui::Begin(ICON_FA_HOME " Welcome to Fufu Engine", nullptr, flags);

		// Logo
		ImGui::Spacing();
		float titleW = ImGui::CalcTextSize("Fufu Engine").x;
		ImGui::SetCursorPosX((600.f - titleW) * 0.5f);
		ImGui::Text("Fufu Engine");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Tabs : Recent / New
		if (ImGui::BeginTabBar("WelcomeTabs"))
		{
			if (ImGui::BeginTabItem(ICON_FA_REFRESH " Recent"))
			{
				drawRecentProjects(state);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(ICON_FA_PLUS " New Project"))
			{
				drawNewProject(state);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		// Open Button
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open Project...",
			ImVec2(-1, 35.f)))
		{
			NFD::Guard guard;
			NFD::UniquePath path;
			nfdfilteritem_t filter = { "Fufu Project", "fufuproj" };

			if (NFD::OpenDialog(path, &filter, 1) == NFD_OKAY)
			{
				if (pm.openProject(path.get()))
				{
					state.selection.clear();
					Fufu::Application::get().getRenderer().resetAccumulation();
					ImGui::End();

					return true;
				}
			}
		}

		ImGui::End();

		return false;
	}

	void WelcomeScreen::drawRecentProjects(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		auto& recents = pm.getRecentProjects();

		if (recents.isEmpty())
		{
			ImGui::Spacing();
			ImGui::SetCursorPosX(20.f);
			ImGui::TextDisabled("No recent projects.");

			return;
		}

		ImGui::Spacing();
		for (const auto& entry : recents.getEntries())
		{
			std::string label = std::string(ICON_CI_PROJECT "  ") + entry.name;

			if (ImGui::Selectable(label.c_str(), false,
				ImGuiSelectableFlags_None, ImVec2(0, 40.f)))
			{
				if (pm.openProject(entry.path))
				{
					state.selection.clear();
					Fufu::Application::get().getRenderer().resetAccumulation();
				}
			}

			ImGui::SameLine();
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.f);
			ImGui::TextDisabled("%s", entry.path.string().c_str());

			ImGui::Separator();
		}
	}

	void WelcomeScreen::drawNewProject(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();

		ImGui::Spacing();
		ImGui::Text("Project Name");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##projname", m_NewProjectName, sizeof(m_NewProjectName));

		ImGui::Spacing();
		ImGui::Text("Location");
		ImGui::SetNextItemWidth(-60.f);
		ImGui::InputText("##projdir", m_NewProjectDir, sizeof(m_NewProjectDir));
		ImGui::SameLine();

		if (ImGui::Button(ICON_FA_ELLIPSIS_H))
		{
			NFD::Guard guard;
			NFD::UniquePath path;

			if (NFD::PickFolder(path) == NFD_OKAY)
				strncpy_s(m_NewProjectDir, path.get(),
					sizeof(m_NewProjectDir) - 1);
		}

		if (strlen(m_NewProjectDir) > 0)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("Will create: %s/%s/", m_NewProjectDir, m_NewProjectName);
		}

		ImGui::Spacing();
		ImGui::Spacing();

		bool canCreate = strlen(m_NewProjectName) > 0 && strlen(m_NewProjectDir) > 0;

		if (!canCreate)
			ImGui::BeginDisabled();

		if (ImGui::Button(ICON_FA_PLUS " Create Project", ImVec2(-1, 35.f)))
		{
			auto project = pm.createProject(std::filesystem::path(m_NewProjectDir), std::string(m_NewProjectName));

			if (project)
			{
				state.selection.clear();
				Fufu::Application::get().getRenderer().resetAccumulation();
			}
		}

		if (!canCreate)
			ImGui::EndDisabled();
	}

}