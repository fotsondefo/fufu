#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace Fufu
{
	// Thread pool dedicated to background tasks (Assimp import, BVH construction,
	// texture decoding...): avoids blocking the render/UI loop for operations that
	// can take several seconds on a heavy model. Does NOT replace a proper
	// Game/Render/RHI thread split like Unreal/Unity — unnecessary here, since
	// all rendering fits in a single glDispatchCompute per frame with no CPU
	// submission bottleneck to hide. Just a place to do pure CPU work without
	// freezing the application.
	//
	// Contract: `work` runs on a pool thread and MUST NEVER touch OpenGL or
	// mutate the ECS/Scene directly (no synchronization is provided for that).
	// `onMainThread` (optional) is queued and executed on the main thread at
	// the next pollMainThreadCallbacks() — THAT is where GPU uploads or any
	// shared mutation should happen.
	class JobSystem
	{
	public:
		// threadCount <= 0 : hardware_concurrency() - 1, minimum 1 (keeps one
		// core for the main thread).
		void init(int threadCount = -1);
		void shutdown();

		void submit(std::function<void()> work, std::function<void()> onMainThread = nullptr);

		// To be called once per frame on the main thread (see
		// Application::run()): executes the onMainThread callbacks of jobs
		// completed since the last call.
		void pollMainThreadCallbacks();

		// Jobs queued or currently running — used only to display a loading
		// indicator in the editor.
		int getPendingJobCount() const { return m_PendingJobs.load(); }

		// For a background thread graph/timeline (see ProfilerPanel):
		// number of workers and whether each one is currently executing a job.
		int  getWorkerCount() const { return static_cast<int>(m_WorkerBusy.size()); }
		bool isWorkerBusy(int index) const
		{
			return index >= 0 && index < static_cast<int>(m_WorkerBusy.size()) && m_WorkerBusy[static_cast<std::size_t>(index)].load();
		}

	private:
		void workerLoop(int index);

		struct Job
		{
			std::function<void()> work;
			std::function<void()> onMainThread;
		};

		std::vector<std::thread> m_Workers;
		std::vector<std::atomic<bool>> m_WorkerBusy; // indexed like m_Workers, built once (never reallocated)
		std::atomic<bool> m_Stop{ false };

		std::queue<Job> m_PendingQueue;
		std::mutex m_PendingMutex;
		std::condition_variable m_PendingCV;

		std::queue<std::function<void()>> m_CompletedQueue;
		std::mutex m_CompletedMutex;

		std::atomic<int> m_PendingJobs{ 0 };
	};
}
