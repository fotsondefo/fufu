#include "Panels/RendererSettingsPanel.h"
#include "Helpers/FontIcons.h"
#include <imgui.h>

namespace FufuStudio 
{

	void RendererSettingsPanel::onImGuiRender(EditorState& /*state*/)
	{
		ImGui::Begin(ICON_FA_WRENCH " Renderer Settings##rendersettings");

		auto& settings = m_Renderer.getSettings();
		bool  changed = false;

		// Mode
		ImGui::SeparatorText("Mode");
		int mode = static_cast<int>(settings.mode);
		if (ImGui::RadioButton("Accumulation", &mode, 0)) { settings.mode = Fufu::RenderMode::Accumulation; changed = true; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Realtime", &mode, 1)) { settings.mode = Fufu::RenderMode::Realtime;     changed = true; }

		// Accumulation info
		if (settings.mode == Fufu::RenderMode::Accumulation)
		{
			ImGui::Text("Frames accumulated: %d / %d",
				m_Renderer.getAccumulatedFrames(),
				settings.maxAccumFrames);
			if (ImGui::Button("Reset accumulation"))
				m_Renderer.resetAccumulation();
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Path Tracing");

		if (ImGui::SliderInt("Max Bounces", &settings.maxBounces, 1, 32))          changed = true;
		if (ImGui::SliderInt("Samples / frame", &settings.samplesPerPixel, 1, 16)) changed = true;
		if (ImGui::SliderInt("Max accum frames", &settings.maxAccumFrames, 64, 8192)) changed = true;

		ImGui::Spacing();
		ImGui::SeparatorText("Post-process");
		if (ImGui::SliderFloat("Exposure", &settings.exposure, 0.1f, 10.f)) changed = true;

		ImGui::Spacing();
		ImGui::SeparatorText("Camera");
		// Ces sliders modifient directement l'EditorState via le pointeur passé à onUpdate
		// — on les met ici pour regrouper les settings
		ImGui::Text("Move speed and look speed");
		ImGui::Text("are in the Viewport panel (WASD + RMB).");

		// Reset automatique si un paramètre change en mode accumulation
		if (changed && settings.mode == Fufu::RenderMode::Accumulation)
			m_Renderer.resetAccumulation();

		ImGui::End();
	}

} // namespace FufuStudio