#pragma once

#include "IEditorPanel.h"
#include <Renderer/Renderer.h>
#include <vector>

namespace FufuStudio
{
	// Lit Fufu::Profiler (collecte engine-side, voir Application::run()) et
	// dessine les graphes/compteurs — ne collecte rien lui-même, c'est
	// délibéré : le futur FufuProfiler (rapport JSON en CI) lira la même
	// donnée sans dupliquer l'instrumentation.
	class ProfilerPanel : public IEditorPanel
	{
	public:
		explicit ProfilerPanel(Fufu::Renderer& renderer) : m_Renderer(renderer) {}

		void onImGuiRender(EditorState& state) override;

	private:
		Fufu::Renderer& m_Renderer;

		// Buffers réutilisés d'une frame à l'autre pour éviter de réallouer
		// à chaque appel d'ImGui::PlotLines.
		std::vector<float> m_CpuPlotBuffer;
		std::vector<float> m_GpuPlotBuffer;
	};
}
