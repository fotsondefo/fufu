#include "Panels/RendererSettingsPanel.h"
#include "Helpers/FontIcons.h"
#include "Helpers/AssetDrop.h"
#include <imgui.h>

namespace FufuStudio 
{

	void RendererSettingsPanel::onImGuiRender(EditorState& state)
	{
		ImGui::Begin(ICON_FA_WRENCH " Renderer Settings##rendersettings");

		auto& settings = m_Renderer.getSettings();
		bool  changed = false;

		// Technique
		ImGui::SeparatorText("Technique");
		int technique = static_cast<int>(settings.technique);
		if (ImGui::RadioButton("Path Tracing", &technique, 0)) { settings.technique = Fufu::RenderTechnique::PathTracing; changed = true; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Ray Tracing", &technique, 1))  { settings.technique = Fufu::RenderTechnique::RayTracing;  changed = true; }

		if (settings.technique == Fufu::RenderTechnique::PathTracing)
			ImGui::TextDisabled("GI diffuse stochastique : réaliste, bruit qui converge avec le temps.");
		else
			ImGui::TextDisabled("Eclairage direct + reflets/refraction, deterministe : rapide, pas de bruit, pas de GI.");

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
		ImGui::SeparatorText("Anti-aliasing");
		int aaMode = static_cast<int>(settings.aaMode);
		if (ImGui::RadioButton("None", &aaMode, 0)) { settings.aaMode = Fufu::AAMode::None; changed = true; }
		ImGui::SameLine();
		if (ImGui::RadioButton("SSAA", &aaMode, 1)) { settings.aaMode = Fufu::AAMode::SSAA; changed = true; }
		ImGui::SameLine();
		if (ImGui::RadioButton("TAA", &aaMode, 2))  { settings.aaMode = Fufu::AAMode::TAA;  changed = true; }
		ImGui::SameLine();
		if (ImGui::RadioButton("FXAA", &aaMode, 3)) { settings.aaMode = Fufu::AAMode::FXAA; changed = true; }

		switch (settings.aaMode)
		{
		case Fufu::AAMode::None:
			ImGui::TextDisabled("Pas de lissage : bords crantés, le moins cher.");
			break;
		case Fufu::AAMode::SSAA:
			ImGui::TextDisabled("Supersampling : jitter par sample (voir 'Samples / frame' ci-dessus), moyenné.");
			break;
		case Fufu::AAMode::TAA:
			ImGui::TextDisabled("Lissage temporel : un sample/frame, moyenné avec l'historique. Fonctionne aussi en Realtime.");
			if (ImGui::SliderFloat("History weight", &settings.taaBlendFactor, 0.f, 0.98f)) changed = true;
			break;
		case Fufu::AAMode::FXAA:
			ImGui::TextDisabled("Post-process par détection de contraste : le moins coûteux, pas de sample supplémentaire.");
			break;
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Post-process");
		if (ImGui::SliderFloat("Exposure", &settings.exposure, 0.1f, 10.f)) changed = true;

		ImGui::Spacing();
		ImGui::SeparatorText("Environment");
		if (auto scene = state.getActiveScene())
		{
			auto& env = scene->getEnvironment();

			if (ImGui::Checkbox("Use Skybox", &env.useSkybox))
				m_Renderer.resetAccumulation();

			std::string label = env.skyboxTexturePath.empty() ? "(none)" : env.skyboxTexturePath;
			ImGui::TextDisabled("%s", label.c_str());

			ImGui::Button(ICON_FA_FILE_IMAGE_O " Drop HDRI texture here", ImVec2(-1, 30.f));
			if (ImGui::BeginDragDropTarget())
			{
				if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Texture)
				{
					env.skyboxTexturePath = meta->sourcePath.string();
					env.useSkybox = true;
					m_Renderer.resetAccumulation();
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::SliderFloat("Skybox Intensity", &env.skyboxIntensity, 0.f, 10.f))
				m_Renderer.resetAccumulation();
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Camera");
		// Ces sliders modifient directement l'EditorState via le pointeur pass� � onUpdate
		// � on les met ici pour regrouper les settings
		ImGui::Text("Move speed and look speed");
		ImGui::Text("are in the Viewport panel (WASD + RMB).");

		// Reset automatique si un param�tre change en mode accumulation
		if (changed && settings.mode == Fufu::RenderMode::Accumulation)
			m_Renderer.resetAccumulation();

		// Reporte les changements sur la scène active pour qu'ils survivent à
		// un Save : `settings` n'est que la copie "live" tenue par le Renderer,
		// la persistance passe par Scene::getRenderSettings() (voir SceneSerializer).
		if (changed)
		{
			if (auto scene = state.getActiveScene())
				scene->getRenderSettings() = settings;
		}

		ImGui::End();
	}

} // namespace FufuStudio