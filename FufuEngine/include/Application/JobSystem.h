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
	// Pool de threads dédié aux tâches de fond (import Assimp, construction de
	// BVH, décodage de texture...) : évite de bloquer la boucle de rendu/UI
	// pour des opérations qui peuvent prendre plusieurs secondes sur un
	// modèle lourd. Ne remplace PAS un vrai découpage Game/Render/RHI thread
	// à la Unreal/Unity — inutile ici, tout le rendu tient en un seul
	// glDispatchCompute par frame, il n'y a pas de goulot de soumission CPU à
	// masquer. Juste un endroit où faire du travail CPU pur sans geler l'appli.
	//
	// Contrat : `work` s'exécute sur un thread du pool et NE DOIT JAMAIS
	// toucher OpenGL ni muter l'ECS/la Scene directement (aucune synchro
	// n'est faite pour ça). `onMainThread` (optionnel) est mis en file et
	// exécuté sur le thread principal au prochain pollMainThreadCallbacks() —
	// c'est LÀ qu'il faut faire l'upload GPU ou toute mutation partagée.
	class JobSystem
	{
	public:
		// threadCount <= 0 : hardware_concurrency() - 1, minimum 1 (garde un
		// cœur pour le thread principal).
		void init(int threadCount = -1);
		void shutdown();

		void submit(std::function<void()> work, std::function<void()> onMainThread = nullptr);

		// À appeler une fois par frame, sur le thread principal (voir
		// Application::run()) : exécute les onMainThread des jobs terminés
		// depuis le dernier appel.
		void pollMainThreadCallbacks();

		// Jobs en file ou en cours d'exécution — sert juste à afficher un
		// indicateur de chargement dans l'éditeur.
		int getPendingJobCount() const { return m_PendingJobs.load(); }

		// Pour un graphe/timeline des threads de fond (voir ProfilerPanel) :
		// nombre de workers, et si chacun exécute un job en ce moment.
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
		std::vector<std::atomic<bool>> m_WorkerBusy; // indexé comme m_Workers, construit une fois (jamais réalloué)
		std::atomic<bool> m_Stop{ false };

		std::queue<Job> m_PendingQueue;
		std::mutex m_PendingMutex;
		std::condition_variable m_PendingCV;

		std::queue<std::function<void()>> m_CompletedQueue;
		std::mutex m_CompletedMutex;

		std::atomic<int> m_PendingJobs{ 0 };
	};
}
