#include "depch.h"
#include "Project/Scene/SceneManager.h"
#include "Project/Components.h"

namespace Fufu 
{

	SceneManager::SceneManager(const std::filesystem::path& scenesDir): m_ScenesDir(scenesDir)
	{
		std::filesystem::create_directories(scenesDir);
		FUFU_INFO("SceneManager created � scenes dir: '{}'", scenesDir.string());
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

		// Une scène sans caméra ne peut rien afficher (voir Renderer::renderScene,
		// qui sort en avance faute de caméra primaire) : on démarre toujours
		// avec une caméra par défaut plutôt que de laisser l'utilisateur
		// découvrir un viewport figé sur la scène précédente.
		Entity cam = scene->createEntity("Camera");
		cam.getComponent<TransformComponent>().position = { 0.f, 1.f, 5.f };
		cam.addComponent<CameraComponent>().primary = true;

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

	bool SceneManager::renameScene(const std::string& oldName, const std::string& newName)
	{
		if (oldName == newName) return true;
		if (newName.empty()) return false;
		if (m_LoadedScenes.count(newName)) return false; // collision de nom

		auto it = m_LoadedScenes.find(oldName);
		if (it == m_LoadedScenes.end()) return false;

		auto scene = it->second;
		m_LoadedScenes.erase(it);
		scene->setName(newName);
		m_LoadedScenes[newName] = scene;

		FUFU_INFO("SceneManager: renamed '{}' -> '{}'", oldName, newName);
		return true;
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