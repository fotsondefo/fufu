#include "depch.h"
#include "Application/JobSystem.h"

namespace Fufu
{

	void JobSystem::init(int threadCount)
	{
		if (threadCount <= 0)
		{
			unsigned hw = std::thread::hardware_concurrency();
			threadCount = static_cast<int>(hw > 1 ? hw - 1 : 1);
		}

		m_Stop = false;

		// Constructed at final size in one shot: a vector<atomic<bool>>
		// cannot be reallocated (atomic is neither copyable nor movable),
		// so no progressive reserve()+emplace_back() like for m_Workers.
		m_WorkerBusy = std::vector<std::atomic<bool>>(static_cast<std::size_t>(threadCount));

		m_Workers.reserve(static_cast<std::size_t>(threadCount));
		for (int i = 0; i < threadCount; ++i)
			m_Workers.emplace_back([this, i]() { workerLoop(i); });

		FUFU_INFO("JobSystem: {} background thread(s) just started", threadCount);
	}

	void JobSystem::shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(m_PendingMutex);
			m_Stop = true;
		}
		m_PendingCV.notify_all();

		for (std::thread& t : m_Workers)
			if (t.joinable()) t.join();

		m_Workers.clear();
	}

	void JobSystem::submit(std::function<void()> work, std::function<void()> onMainThread)
	{
		++m_PendingJobs;
		{
			std::lock_guard<std::mutex> lock(m_PendingMutex);
			m_PendingQueue.push(Job{ std::move(work), std::move(onMainThread) });
		}
		m_PendingCV.notify_one();
	}

	void JobSystem::workerLoop(int index)
	{
		for (;;)
		{
			Job job;
			{
				std::unique_lock<std::mutex> lock(m_PendingMutex);
				m_PendingCV.wait(lock, [this]() { return m_Stop.load() || !m_PendingQueue.empty(); });

				if (m_Stop.load() && m_PendingQueue.empty())
					return;

				job = std::move(m_PendingQueue.front());
				m_PendingQueue.pop();
			}

			m_WorkerBusy[static_cast<std::size_t>(index)] = true;
			if (job.work)
				job.work();
			m_WorkerBusy[static_cast<std::size_t>(index)] = false;

			if (job.onMainThread)
			{
				std::lock_guard<std::mutex> lock(m_CompletedMutex);
				m_CompletedQueue.push(std::move(job.onMainThread));
			}

			--m_PendingJobs;
		}
	}

	void JobSystem::pollMainThreadCallbacks()
	{
		// Drain the queue into a local vector under lock, then execute the
		// callbacks outside the lock (a callback could itself want to
		// submit a new job).
		std::vector<std::function<void()>> callbacks;
		{
			std::lock_guard<std::mutex> lock(m_CompletedMutex);
			while (!m_CompletedQueue.empty())
			{
				callbacks.push_back(std::move(m_CompletedQueue.front()));
				m_CompletedQueue.pop();
			}
		}

		for (auto& cb : callbacks)
			cb();
	}

}
