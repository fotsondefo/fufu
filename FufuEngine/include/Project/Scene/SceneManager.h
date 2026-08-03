#pragma once

#include "Scene.h"
#include "SceneSerializer.h"
#include "Core/Signal.h"
#include <filesystem>
#include <string>
#include <memory>
#include <unordered_map>

namespace Fufu 
{

	class SceneManager
	{
	public:
		explicit SceneManager(const std::filesystem::path& scenesDir);
		~SceneManager() = default;

		std::shared_ptr<Scene> loadScene(const std::filesystem::path& path);
		std::shared_ptr<Scene> newScene(const std::string& name = "Untitled");
		bool saveScene(std::shared_ptr<Scene> scene,
			const std::filesystem::path& path);
		void unloadScene(const std::string& name);
		void unloadAll();

		// Renames a loaded scene (map key + Scene::m_Name). Fails (returns false)
		// if newName is already taken by another loaded scene.
		bool renameScene(const std::string& oldName, const std::string& newName);

		void setActiveScene(const std::string& name);
		std::shared_ptr<Scene> getActiveScene() const;
		bool hasActiveScene() const { return m_ActiveScene != nullptr; }

		const std::unordered_map<std::string, std::shared_ptr<Scene>>& getLoadedScenes() const
		{
			return m_LoadedScenes;
		}

		const std::filesystem::path& getScenesDir() const { return m_ScenesDir; }

		// Emitted when setActiveScene() changes the current scene.
		// Scene* can be null if the active scene is deactivated without a replacement.
		Signal<Scene&> onSceneActivated;
		Signal<>       onSceneClosed;

	private:
		std::filesystem::path m_ScenesDir;
		std::unordered_map<std::string, std::shared_ptr<Scene>> m_LoadedScenes;
		std::shared_ptr<Scene> m_ActiveScene;
	};

}