#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>

namespace Fufu
{
	// Non-blocking GPU timer: multi-buffering of GL_TIME_ELAPSED queries
	// to never stall while waiting for a result — begin()/end() post a query,
	// getLastResultMs() returns the most recent result already available
	// (potentially a few frames old, which is acceptable for a live perf graph).
	class GPUTimerQuery
	{
	public:
		void init();
		void shutdown();
		void begin();
		void end();
		float getLastResultMs();

	private:
		static constexpr int k_QueryCount = 3;
		uint32_t m_Queries[k_QueryCount] = {};
		bool     m_HasPending[k_QueryCount] = {};
		int      m_WriteIndex = 0;
		float    m_LastResultMs = 0.f;
	};

	// What was measured for ONE frame — presentation-agnostic: read by both
	// ProfilerPanel (editor, ImGui graphs) and, later, by FufuProfiler
	// (JSON report in CI). Collected once here.
	struct ProfilerFrame
	{
		float cpuFrameTimeMs = 0.f;
		std::unordered_map<std::string, float> gpuSectionsMs; // "ComputePass" -> ms measured on GPU
		int   triangleCount = 0; // unique triangles in BLAS (not repeated per instance)
		int   instanceCount = 0;
		int   drawCallCount = 0; // dispatches/draws for the frame (1 compute + 1 if FXAA active)
	};

	// Collects CPU/GPU/scene metrics. Simple singleton (consistent with
	// Application::get()): one Profiler per process, whether it is
	// FufuStudio or the future headless FufuProfiler.
	class Profiler
	{
	public:
		static Profiler& get();

		void init();
		void shutdown();

		// To be called once per frame, wrapping all the work to be measured.
		void beginFrame();
		void endFrame();

		// To be called around a GPU dispatch/draw to measure (see ComputePass,
		// FXAAPass) — free name, a GPUTimerQuery is created on the first call.
		void beginGPU(const std::string& section);
		void endGPU(const std::string& section);

		void setCounters(int triangleCount, int instanceCount, int drawCallCount);

		// Returns the LAST fully measured frame (the one pushed by the previous
		// endFrame()), not m_Current which is still being filled — panels/overlays
		// (ViewportPanel, ProfilerPanel) are displayed DURING the frame, before
		// endFrame() has computed cpuFrameTimeMs for THIS frame: reading m_Current
		// directly would always show values freshly reset by beginFrame().
		// One frame behind, imperceptible for a live display.
		const ProfilerFrame& getCurrentFrame() const
		{
			static const ProfilerFrame s_Empty;
			return m_History.empty() ? s_Empty : m_History.back();
		}
		const std::vector<ProfilerFrame>& getHistory() const { return m_History; }
		float getFPS() const
		{
			float ms = getCurrentFrame().cpuFrameTimeMs;
			return ms > 0.f ? 1000.f / ms : 0.f;
		}

	private:
		ProfilerFrame m_Current;
		std::vector<ProfilerFrame> m_History; // ring buffer (FIFO), see k_HistorySize
		static constexpr std::size_t k_HistorySize = 240;

		std::unordered_map<std::string, GPUTimerQuery> m_GPUTimers;
		std::chrono::steady_clock::time_point m_FrameStart;
	};
}
