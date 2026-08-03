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
		if (ImGui::RadioButton("Ray Tracing",  &technique, 1)) { settings.technique = Fufu::RenderTechnique::RayTracing;  changed = true; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Forward",      &technique, 2)) { settings.technique = Fufu::RenderTechnique::Forward;     changed = true; }
		ImGui::SameLine();
		if (ImGui::RadioButton("Deferred",     &technique, 3)) { settings.technique = Fufu::RenderTechnique::Deferred;    changed = true; }

		switch (settings.technique)
		{
		case Fufu::RenderTechnique::PathTracing:
			ImGui::TextDisabled("Stochastic diffuse GI: realistic, noise converges over time.");
			break;
		case Fufu::RenderTechnique::RayTracing:
			ImGui::TextDisabled("Direct lighting + reflections/refraction, deterministic: fast, no noise, no GI.");
			break;
		case Fufu::RenderTechnique::Forward:
			ImGui::TextDisabled("Direct PBR rasterisation: Cook-Torrance lighting per fragment, real-time.");
			break;
		case Fufu::RenderTechnique::Deferred:
			ImGui::TextDisabled("G-Buffer then fullscreen lighting: optimal when there are many lights.");
			break;
		}

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
			ImGui::TextDisabled("No smoothing: jagged edges, cheapest.");
			break;
		case Fufu::AAMode::SSAA:
			ImGui::TextDisabled("Supersampling: jitter per sample (see 'Samples / frame' above), averaged.");
			break;
		case Fufu::AAMode::TAA:
			ImGui::TextDisabled("Temporal smoothing: one sample/frame, averaged with history. Also works in Realtime.");
			if (ImGui::SliderFloat("History weight", &settings.taaBlendFactor, 0.f, 0.98f)) changed = true;
			break;
		case Fufu::AAMode::FXAA:
			ImGui::TextDisabled("Post-process via contrast detection: cheapest, no additional sample.");
			break;
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Post-process");
		if (ImGui::SliderFloat("Exposure", &settings.exposure, 0.1f, 10.f)) changed = true;

		ImGui::Spacing();
		ImGui::SeparatorText("Tone Mapping");

		static const char* s_TmLabels[] = { "None", "Reinhard", "ACES", "Filmic (Uncharted 2)" };
		int tmOp = static_cast<int>(settings.tonemapping);
		if (ImGui::Combo("Operator##tm", &tmOp, s_TmLabels, IM_ARRAYSIZE(s_TmLabels)))
		{
			settings.tonemapping = static_cast<Fufu::ToneMappingOperator>(tmOp);
			changed = true;
		}
		switch (settings.tonemapping)
		{
		case Fufu::ToneMappingOperator::None:
			ImGui::TextDisabled("HDR lineaire + gamma uniquement (debug).");
			break;
		case Fufu::ToneMappingOperator::Reinhard:
			ImGui::TextDisabled("x / (1 + x) : doux, preserve les couleurs a hautes luminances.");
			break;
		case Fufu::ToneMappingOperator::ACES:
			ImGui::TextDisabled("Academy Color Encoding : contraste marque, standard industrie cinema.");
			break;
		case Fufu::ToneMappingOperator::Filmic:
			ImGui::TextDisabled("Uncharted 2 : gamma integre, lift noir leger, look chaud.");
			break;
		}

		if (settings.tonemapping != Fufu::ToneMappingOperator::Filmic)
		{
			if (ImGui::SliderFloat("Gamma##tm", &settings.gamma, 1.f, 3.f)) changed = true;
		}
		else
		{
			ImGui::TextDisabled("Gamma integre dans la courbe Filmic.");
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Bloom");
		if (ImGui::Checkbox("Enable##bloom", &settings.bloomEnabled)) changed = true;
		if (settings.bloomEnabled)
		{
			if (ImGui::SliderFloat("Threshold##bloom", &settings.bloomThreshold, 0.1f, 5.f))  changed = true;
			if (ImGui::SliderFloat("Knee##bloom",      &settings.bloomKnee,      0.f,  1.f))  changed = true;
			if (ImGui::SliderFloat("Strength##bloom",  &settings.bloomStrength,  0.f,  1.f))  changed = true;
			if (ImGui::SliderInt  ("Iterations##bloom",&settings.bloomIterations, 1,    4))    changed = true;
			ImGui::TextDisabled("Flou gaussien a mi-res, %d passes H+V.", settings.bloomIterations * 2);
		}

		// SSAO (deferred uniquement)
		if (settings.technique == Fufu::RenderTechnique::Deferred)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("SSAO");
			ImGui::TextDisabled("Occlusion ambiante en espace ecran (deferred uniquement).");
			if (ImGui::Checkbox("Enable##ssao", &settings.ssaoEnabled)) changed = true;
			if (settings.ssaoEnabled)
			{
				if (ImGui::SliderInt  ("Samples##ssao",  &settings.ssaoSamples,  4,    64))     changed = true;
				if (ImGui::SliderFloat("Radius##ssao",   &settings.ssaoRadius,   0.05f, 2.f))  changed = true;
				if (ImGui::SliderFloat("Bias##ssao",     &settings.ssaoBias,     0.001f, 0.1f, "%.3f")) changed = true;
				if (ImGui::SliderFloat("Strength##ssao", &settings.ssaoStrength, 0.1f,  3.f))  changed = true;
			}
		}

		// Volumetrics (deferred uniquement)
		if (settings.technique == Fufu::RenderTechnique::Deferred)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Volumetric Lighting");
			ImGui::TextDisabled("Raymarching a demi-res, Henyey-Greenstein, shadow map reuse.");
			if (ImGui::Checkbox("Enable##vol", &settings.volEnabled)) changed = true;
			if (settings.volEnabled)
			{
				if (ImGui::SliderInt  ("Steps##vol",      &settings.volSteps,      4,    128))          changed = true;
				if (ImGui::SliderFloat("Density##vol",    &settings.volDensity,    0.001f, 0.5f, "%.3f")) changed = true;
				if (ImGui::SliderFloat("Scattering##vol", &settings.volScattering, 0.f,    1.f))          changed = true;
				if (ImGui::SliderFloat("Anisotropy##vol", &settings.volAnisotropy,-1.f,    1.f, "%.2f"))  changed = true;
				if (ImGui::SliderFloat("Ambient##vol",    &settings.volAmbient,    0.f,    0.1f, "%.3f")) changed = true;
				if (ImGui::SliderFloat("Max Dist##vol",   &settings.volMaxDist,    10.f,  500.f))         changed = true;
				ImGui::TextDisabled("Anisotropy > 0 = god rays, < 0 = contre-jour.");
			}
		}

		// DoF (deferred uniquement)
		if (settings.technique == Fufu::RenderTechnique::Deferred)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Depth of Field");
			ImGui::TextDisabled("Flou bokeh par disque de Vogel (deferred uniquement).");
			if (ImGui::Checkbox("Enable##dof", &settings.dofEnabled)) changed = true;
			if (settings.dofEnabled)
			{
				if (ImGui::SliderFloat("Focus Distance##dof", &settings.dofFocusDist,  0.5f, 100.f)) changed = true;
				if (ImGui::SliderFloat("Focus Range##dof",    &settings.dofFocusRange, 0.1f,  20.f)) changed = true;
				if (ImGui::SliderFloat("Max Blur (px)##dof",  &settings.dofMaxBlur,    1.f,   30.f)) changed = true;
				if (ImGui::SliderInt  ("Samples##dof",        &settings.dofSamples,    4,     32))   changed = true;
				ImGui::TextDisabled("Focus +/- %.1f world units depuis %.1f.", settings.dofFocusRange, settings.dofFocusDist);
			}
		}

		// Ombres (deferred uniquement)
		if (settings.technique == Fufu::RenderTechnique::Deferred)
		{
			ImGui::Spacing();
			ImGui::SeparatorText("Shadow Map");
			ImGui::TextDisabled("Depth map 2048x2048, PCF 3x3, premiere lumiere directionnelle.");
			if (ImGui::SliderFloat("Bias##shadow", &settings.shadowBias, 0.0001f, 0.05f, "%.4f"))
				changed = true;
		}

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
		// These sliders directly modify the EditorState via the pointer passed to onUpdate
		// — they are placed here to group settings together
		ImGui::Text("Move speed and look speed");
		ImGui::Text("are in the Viewport panel (WASD + RMB).");

		// Automatic reset if a parameter changes in accumulation mode
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