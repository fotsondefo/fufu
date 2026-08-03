#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>
#include <vector>

namespace FufuStudio
{
	// Reads Fufu::Profiler (engine-side collection, see Application::run()) and
	// draws graphs/counters — collects nothing itself, by design: the future
	// FufuProfiler (JSON report in CI) will read the same data without
	// duplicating the instrumentation.
	class ProfilerPanel : public IEditorPanel
	{
	public:
		explicit ProfilerPanel(Fufu::Renderer& renderer) : m_Renderer(renderer) {}

		void onImGuiRender(EditorState& state) override;

	private:
		Fufu::Renderer& m_Renderer;

		// Buffers reused frame-to-frame to avoid reallocating
		// on every ImGui::PlotLines call.
		std::vector<float> m_CpuPlotBuffer;
		std::vector<float> m_GpuPlotBuffer;
	};
}
