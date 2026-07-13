#include "Panels/ProfilerPanel.h"
#include "Helpers/FontIcons.h"
#include "Helpers/MeshPicking.h"
#include <Application/Application.h>
#include <Application/Profiler.h>
#include <Project/Components.h>
#include <imgui.h>
#include <cfloat>

namespace FufuStudio
{

	static void fillPlotBuffer(std::vector<float>& buffer, const std::vector<Fufu::ProfilerFrame>& history,
		float Fufu::ProfilerFrame::* field)
	{
		buffer.resize(history.size());
		for (std::size_t i = 0; i < history.size(); ++i)
			buffer[i] = history[i].*field;
	}

	void ProfilerPanel::onImGuiRender(EditorState& state)
	{
		ImGui::Begin(ICON_FA_LINE_CHART " Profiler##profiler");

		auto& profiler = Fufu::Profiler::get();
		const auto& current = profiler.getCurrentFrame();
		const auto& history = profiler.getHistory();

		// ---- Compteurs ----
		ImGui::SeparatorText("Frame");
		ImGui::Text("FPS: %.0f (%.2f ms)", profiler.getFPS(), current.cpuFrameTimeMs);
		ImGui::Text("Draw calls: %d", current.drawCallCount);
		ImGui::Text("Triangles (scene, uniques): %d", current.triangleCount);
		ImGui::Text("Instances: %d", current.instanceCount);

		// Triangles du mesh actuellement sélectionné, séparément du total
		// scène (utile pour juger si UN objet est trop lourd, pas juste la
		// scène entière).
		Fufu::Entity primary = state.selection.primary();
		if (primary && primary.isValid() && primary.hasComponent<Fufu::MeshComponent>())
		{
			auto meshAsset = getMeshAssetForEntity(primary);
			if (meshAsset)
			{
				std::size_t triCount = 0;
				for (const auto& sub : meshAsset->getSubMeshes())
					triCount += sub.indices.size() / 3;

				ImGui::Text("Selected mesh triangles: %zu", triCount);
			}
		}
		else
		{
			ImGui::TextDisabled("Selected mesh triangles: (no mesh selected)");
		}

		// ---- Graphes CPU/GPU ----
		ImGui::Spacing();
		ImGui::SeparatorText("CPU / GPU (historique)");

		fillPlotBuffer(m_CpuPlotBuffer, history, &Fufu::ProfilerFrame::cpuFrameTimeMs);
		if (!m_CpuPlotBuffer.empty())
		{
			ImGui::PlotLines("CPU frame (ms)", m_CpuPlotBuffer.data(), static_cast<int>(m_CpuPlotBuffer.size()),
				0, nullptr, 0.f, FLT_MAX, ImVec2(-1, 60));
		}

		// gpuSectionsMs est une map par frame : on extrait "ComputePass" (et
		// "FXAAPass" s'il est actif) manuellement plutôt que via fillPlotBuffer
		// (qui suppose un champ direct, pas une clé de map).
		m_GpuPlotBuffer.resize(history.size());
		for (std::size_t i = 0; i < history.size(); ++i)
		{
			auto it = history[i].gpuSectionsMs.find("ComputePass");
			m_GpuPlotBuffer[i] = (it != history[i].gpuSectionsMs.end()) ? it->second : 0.f;
		}
		if (!m_GpuPlotBuffer.empty())
		{
			ImGui::PlotLines("GPU ComputePass (ms)", m_GpuPlotBuffer.data(), static_cast<int>(m_GpuPlotBuffer.size()),
				0, nullptr, 0.f, FLT_MAX, ImVec2(-1, 60));
		}

		auto fxaaIt = current.gpuSectionsMs.find("FXAAPass");
		if (fxaaIt != current.gpuSectionsMs.end())
			ImGui::Text("GPU FXAAPass: %.3f ms", fxaaIt->second);

		// ---- Threads de fond ----
		ImGui::Spacing();
		ImGui::SeparatorText("Background threads (JobSystem)");

		auto& jobSystem = Fufu::Application::get().getJobSystem();
		int workerCount = jobSystem.getWorkerCount();
		int pendingJobs = jobSystem.getPendingJobCount();

		ImGui::Text("Pending jobs: %d", pendingJobs);
		for (int i = 0; i < workerCount; ++i)
		{
			bool busy = jobSystem.isWorkerBusy(i);
			ImVec4 color = busy ? ImVec4(0.95f, 0.55f, 0.2f, 1.f) : ImVec4(0.35f, 0.65f, 0.35f, 1.f);
			ImGui::TextColored(color, "Worker %d: %s", i, busy ? "busy" : "idle");
		}

		ImGui::End();
	}

}
