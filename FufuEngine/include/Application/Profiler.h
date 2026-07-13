#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>

namespace Fufu
{
	// Timer GPU non-bloquant : multi-buffering de requêtes GL_TIME_ELAPSED
	// pour ne jamais stall en attendant un résultat — begin()/end() postent
	// une requête, getLastResultMs() renvoie le résultat le plus récent déjà
	// disponible (potentiellement vieux de quelques frames, ce qui est
	// acceptable pour un graphe de perf live).
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

	// Ce qui a été mesuré pour UNE frame — présentation-agnostique : lu à la
	// fois par ProfilerPanel (éditeur, graphes ImGui) et, plus tard, par
	// FufuProfiler (rapport JSON en CI). Collecté une seule fois ici.
	struct ProfilerFrame
	{
		float cpuFrameTimeMs = 0.f;
		std::unordered_map<std::string, float> gpuSectionsMs; // "ComputePass" -> ms mesuré côté GPU
		int   triangleCount = 0; // triangles uniques dans les BLAS (pas répétés par instance)
		int   instanceCount = 0;
		int   drawCallCount = 0; // dispatches/draws de la frame (1 compute + 1 si FXAA actif)
	};

	// Collecte les métriques CPU/GPU/scène. Singleton simple (cohérent avec
	// Application::get()) : un seul Profiler par process, que ce soit
	// FufuStudio ou le futur FufuProfiler headless.
	class Profiler
	{
	public:
		static Profiler& get();

		void init();
		void shutdown();

		// À appeler une fois par frame, autour de tout le travail à mesurer.
		void beginFrame();
		void endFrame();

		// À appeler autour d'un dispatch/draw GPU à mesurer (voir ComputePass,
		// FXAAPass) — nom libre, un GPUTimerQuery est créé au premier appel.
		void beginGPU(const std::string& section);
		void endGPU(const std::string& section);

		void setCounters(int triangleCount, int instanceCount, int drawCallCount);

		// Renvoie la DERNIÈRE frame entièrement mesurée (celle poussée par le
		// endFrame() précédent), pas m_Current qui est encore en cours de
		// remplissage — les panels/overlays (ViewportPanel, ProfilerPanel)
		// s'affichent PENDANT la frame, avant que endFrame() n'ait calculé
		// cpuFrameTimeMs de CETTE frame : lire m_Current directement y verrait
		// toujours les valeurs fraîchement remises à zéro par beginFrame().
		// Décalage d'une frame, imperceptible pour un affichage live.
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
		std::vector<ProfilerFrame> m_History; // ring buffer (FIFO), voir k_HistorySize
		static constexpr std::size_t k_HistorySize = 240;

		std::unordered_map<std::string, GPUTimerQuery> m_GPUTimers;
		std::chrono::steady_clock::time_point m_FrameStart;
	};
}
