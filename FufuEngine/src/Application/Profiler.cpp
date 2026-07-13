#include "depch.h"
#include "Application/Profiler.h"

namespace Fufu
{

	void GPUTimerQuery::init()
	{
		glGenQueries(k_QueryCount, m_Queries);
	}

	void GPUTimerQuery::shutdown()
	{
		glDeleteQueries(k_QueryCount, m_Queries);
	}

	void GPUTimerQuery::begin()
	{
		glBeginQuery(GL_TIME_ELAPSED, m_Queries[m_WriteIndex]);
	}

	void GPUTimerQuery::end()
	{
		glEndQuery(GL_TIME_ELAPSED);
		m_HasPending[m_WriteIndex] = true;
		m_WriteIndex = (m_WriteIndex + 1) % k_QueryCount;
	}

	float GPUTimerQuery::getLastResultMs()
	{
		// Parcourt les requêtes en attente et récupère celles déjà prêtes,
		// SANS jamais attendre (GL_QUERY_RESULT_AVAILABLE) — sinon on stall
		// le pipeline GPU, exactement ce que le multi-buffering évite.
		for (int i = 0; i < k_QueryCount; ++i)
		{
			int idx = (m_WriteIndex + i) % k_QueryCount;
			if (!m_HasPending[idx]) continue;

			GLint available = 0;
			glGetQueryObjectiv(m_Queries[idx], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available)
			{
				GLuint64 timeNs = 0;
				glGetQueryObjectui64v(m_Queries[idx], GL_QUERY_RESULT, &timeNs);
				m_LastResultMs = static_cast<float>(timeNs) / 1000000.f;
				m_HasPending[idx] = false;
			}
		}
		return m_LastResultMs;
	}

	Profiler& Profiler::get()
	{
		static Profiler s_Instance;
		return s_Instance;
	}

	void Profiler::init()
	{
		m_History.reserve(k_HistorySize);
	}

	void Profiler::shutdown()
	{
		for (auto& [name, timer] : m_GPUTimers)
			timer.shutdown();
		m_GPUTimers.clear();
	}

	void Profiler::beginFrame()
	{
		m_FrameStart = std::chrono::steady_clock::now();
		m_Current = ProfilerFrame{};
	}

	void Profiler::endFrame()
	{
		auto elapsed = std::chrono::steady_clock::now() - m_FrameStart;
		m_Current.cpuFrameTimeMs = std::chrono::duration<float, std::milli>(elapsed).count();

		for (auto& [name, timer] : m_GPUTimers)
			m_Current.gpuSectionsMs[name] = timer.getLastResultMs();

		// Ring buffer FIFO : coût O(n) par frame sur erase(begin()), mais
		// n <= k_HistorySize (240) — négligeable une fois par frame.
		if (m_History.size() >= k_HistorySize)
			m_History.erase(m_History.begin());
		m_History.push_back(m_Current);
	}

	void Profiler::beginGPU(const std::string& section)
	{
		auto it = m_GPUTimers.find(section);
		if (it == m_GPUTimers.end())
		{
			GPUTimerQuery timer;
			timer.init();
			it = m_GPUTimers.emplace(section, std::move(timer)).first;
		}
		it->second.begin();
	}

	void Profiler::endGPU(const std::string& section)
	{
		auto it = m_GPUTimers.find(section);
		if (it != m_GPUTimers.end())
			it->second.end();
	}

	void Profiler::setCounters(int triangleCount, int instanceCount, int drawCallCount)
	{
		m_Current.triangleCount = triangleCount;
		m_Current.instanceCount = instanceCount;
		m_Current.drawCallCount = drawCallCount;
	}

}
