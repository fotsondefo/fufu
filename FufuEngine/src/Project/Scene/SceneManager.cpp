#include "depch.h"
#include "Project/Scene/SceneManager.h"

namespace Fufu 
{

	SceneManager::SceneManager(const std::filesystem::path& scenesDir): m_ScenesDir(scenesDir)
	{
		std::filesystem::create_directories(scenesDir);
		FUFU_INFO("SceneManager created — scenes dir: '{}'", scenesDir.string());
	}

	std::shared_ptr<Scene> SceneManager::loadScene(const std::filesystem::path& path)
	{
		auto scene = std::make_shared<Scene>();
		SceneSerializer serializer(scene.get());

		if (!serializer.deserialize(path))
		{
			FUFU_ERROR("SceneManager: failed to load '{}'", path.string());
			return nullptr;
		}

		m_LoadedScenes[scene->getName()] = scene;
		FUFU_INFO("SceneManager: loaded '{}'", scene->getName());

		return scene;
	}

	std::shared_ptr<Scene> SceneManager::newScene(const std::string& name)
	{
		auto scene = std::make_shared<Scene>(name);
		m_LoadedScenes[name] = scene;
		FUFU_INFO("SceneManager: new scene '{}'", name);

		return scene;
	}

	bool SceneManager::saveScene(std::shared_ptr<Scene> scene, const std::filesystem::path& path)
	{
		if (!scene) return false;

		std::filesystem::create_directories(path.parent_path());
		SceneSerializer serializer(scene.get());
		serializer.serialize(path);
		FUFU_INFO("SceneManager: saved '{}' to '{}'", scene->getName(), path.string());

		return true;
	}

	void SceneManager::unloadScene(const std::string& name)
	{
		auto it = m_LoadedScenes.find(name);
		if (it == m_LoadedScenes.end()) return;

		if (m_ActiveScene == it->second)
			m_ActiveScene = nullptr;

		m_LoadedScenes.erase(it);
		FUFU_INFO("SceneManager: unloaded '{}'", name);
	}

	void SceneManager::unloadAll()
	{
		m_LoadedScenes.clear();
		m_ActiveScene = nullptr;
		FUFU_INFO("SceneManager: all scenes unloaded");
	}

	void SceneManager::setActiveScene(const std::string& name)
	{
		auto it = m_LoadedScenes.find(name);
		if (it == m_LoadedScenes.end())
		{
			FUFU_ERROR("SceneManager: scene '{}' not loaded", name);
			return;
		}
		m_ActiveScene = it->second;
		FUFU_INFO("SceneManager: active scene ? '{}'", name);
	}

	std::shared_ptr<Scene> SceneManager::getActiveScene() const
	{
		return m_ActiveScene;
	}

}