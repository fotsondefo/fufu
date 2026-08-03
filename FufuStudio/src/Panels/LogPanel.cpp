#include "Panels/LogPanel.h"
#include "Application/ImGuiSink.h"
#include "Helpers/FontIcons.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace FufuStudio
{

void LogPanel::onImGuiRender(EditorState& /*state*/)
{
    ImGui::Begin(ICON_FA_TERMINAL " Log##logpanel");

    // ── Toolbar ───────────────────────────────────────────────────────────
    if (ImGui::SmallButton("Clear"))
    {
        auto& sink = Fufu::ImGuiSink_mt::instance();
        if (sink) sink->clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Trace",  &m_ShowTrace);
    ImGui::SameLine();
    ImGui::Checkbox("Info",   &m_ShowInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warn",   &m_ShowWarn);
    ImGui::SameLine();
    ImGui::Checkbox("Error",  &m_ShowError);
    ImGui::SameLine();
    ImGui::Checkbox("Scroll", &m_AutoScroll);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.f);
    ImGui::InputText(ICON_FA_SEARCH "##logfilter", m_Filter, sizeof(m_Filter));

    ImGui::Separator();

    // ── Log entries ────────────────────────────────────────────────────────
    ImGui::BeginChild("##logscroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    auto& sink = Fufu::ImGuiSink_mt::instance();
    if (sink)
    {
        const auto& entries = sink->entries();
        const std::string filterStr(m_Filter);

        for (const auto& e : entries)
        {
            using lvl = spdlog::level::level_enum;

            if (e.level == lvl::trace   && !m_ShowTrace) continue;
            if (e.level == lvl::info    && !m_ShowInfo)  continue;
            if (e.level == lvl::warn    && !m_ShowWarn)  continue;
            if ((e.level == lvl::err || e.level == lvl::critical) && !m_ShowError) continue;
            if (!filterStr.empty() && e.text.find(filterStr) == std::string::npos) continue;

            ImVec4 col;
            switch (e.level)
            {
            case lvl::trace:    col = ImVec4(0.5f, 0.5f, 0.5f, 1.f); break;
            case lvl::info:     col = ImVec4(0.9f, 0.9f, 0.9f, 1.f); break;
            case lvl::warn:     col = ImVec4(1.0f, 0.8f, 0.2f, 1.f); break;
            case lvl::err:      col = ImVec4(1.0f, 0.3f, 0.3f, 1.f); break;
            case lvl::critical: col = ImVec4(1.0f, 0.0f, 0.5f, 1.f); break;
            default:            col = ImVec4(0.9f, 0.9f, 0.9f, 1.f); break;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(e.text.c_str());
            ImGui::PopStyleColor();
        }

        if (m_AutoScroll && sink->hasNew())
        {
            ImGui::SetScrollHereY(1.f);
            sink->clearNew();
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace FufuStudio
