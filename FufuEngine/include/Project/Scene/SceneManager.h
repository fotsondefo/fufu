#pragma once

#include "Scene.h"
#include "SceneSerializer.h"
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

		void setActiveScene(const std::string& name);
		std::shared_ptr<Scene> getActiveScene() const;
		bool hasActiveScene() const { return m_ActiveScene != nullptr; }

		const std::unordered_map<std::string, std::shared_ptr<Scene>>& getLoadedScenes() const
		{
			return m_LoadedScenes;
		}

		const std::filesystem::path& getScenesDir() const { return m_ScenesDir; }

	private:
		std::filesystem::path m_ScenesDir;
		std::unordered_map<std::string, std::shared_ptr<Scene>> m_LoadedScenes;
		std::shared_ptr<Scene> m_ActiveScene;
	};

}