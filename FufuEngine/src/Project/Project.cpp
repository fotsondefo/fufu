#include "depch.h"
#include "Project/Project.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

namespace Fufu 
{

	Project::Project(const ProjectInfo& info)
		: m_Info(info)
		, m_AssetManager(std::make_unique<AssetManager>(info.assetsDir()))
		, m_SceneManager(std::make_unique<SceneManager>(info.scenesDir()))
	{}

	void Project::init()
	{
		// Create the directory structure if it's a new project
		std::filesystem::create_directories(m_Info.assetsDir() / "Meshes");
		std::filesystem::create_directories(m_Info.assetsDir() / "Textures");
		std::filesystem::create_directories(m_Info.assetsDir() / "Materials");
		std::filesystem::create_directories(m_Info.assetsDir() / "Shaders");
		std::filesystem::create_directories(m_Info.scenesDir());
		std::filesystem::create_directories(m_Info.prefabsDir());
		std::filesystem::create_directories(m_Info.configDir() / "editor");

		// Scan existing assets
		m_AssetManager->scanDirectory();

		// Charge TOUTES les scènes connues du projet (pas seulement
		// startupScene, qui ne servait auparavant qu'à choisir la scène active
		// — les autres entrées de m_Info.scenes n'étaient jamais lues).
		std::string activeSceneName;
		for (const std::string& relPath : m_Info.scenes)
		{
			std::filesystem::path scenePath = m_Info.rootDirectory / relPath;
			if (!std::filesystem::exists(scenePath))
				continue;

			auto scene = m_SceneManager->loadScene(scenePath);
			if (!scene) continue;

			if (activeSceneName.empty() || relPath == m_Info.startupScene)
				activeSceneName = scene->getName();
		}

		if (!activeSceneName.empty())
		{
			m_SceneManager->setActiveScene(activeSceneName);
		}
		else
		{
			// Projet neuf, ou tous les fichiers de scène du manifeste sont
			// manquants : scène par défaut, sauvegardée immédiatement (et
			// enregistrée dans le manifeste) pour ne pas disparaître au
			// prochain lancement si l'utilisateur ne fait jamais Save.
			auto scene = m_SceneManager->newScene("Main");
			m_SceneManager->setActiveScene("Main");

			std::filesystem::path path = m_Info.scenesDir() / "Main.fufuscene";
			if (m_SceneManager->saveScene(scene, path))
				registerScene("Scenes/Main.fufuscene");
		}

		FUFU_INFO("Project '{}' initialized", m_Info.name);
	}

	void Project::registerScene(const std::string& relativePath)
	{
		if (std::find(m_Info.scenes.begin(), m_Info.scenes.end(), relativePath) != m_Info.scenes.end())
			return;

		m_Info.scenes.push_back(relativePath);
		save();
	}

	void Project::saveAllLoadedScenes()
	{
		for (auto& [name, scene] : m_SceneManager->getLoadedScenes())
		{
			std::filesystem::path path = m_Info.scenesDir() / (name + ".fufuscene");
			if (m_SceneManager->saveScene(scene, path))
			{
				std::filesystem::path rel = std::filesystem::relative(path, m_Info.rootDirectory);
				registerScene(rel.generic_string());
			}
		}
	}

	void Project::save() const
	{
		json j;
		j["name"] = m_Info.name;
		j["version"] = m_Info.version;
		j["startupScene"] = m_Info.startupScene;
		j["scenes"] = m_Info.scenes;
		j["assetDirectories"] = m_Info.assetDirectories;

		std::ofstream file(m_Info.projectFilePath);
		FUFU_ASSERT(file.is_open(), "Failed to save project file: {}", m_Info.projectFilePath.string());

		file << j.dump(4);
		FUFU_INFO("Project saved: '{}'", m_Info.projectFilePath.string());
	}

	std::shared_ptr<Project> Project::load(const std::filesystem::path& projFilePath)
	{
		std::ifstream file(projFilePath);
		if (!file.is_open())
		{
			FUFU_ERROR("Cannot open project file: '{}'", projFilePath.string());
			return nullptr;
		}

		json j;
		try { j = json::parse(file); }
		catch (const json::parse_error& e)
		{
			FUFU_ERROR("Project parse error: {}", e.what());
			return nullptr;
		}

		ProjectInfo info;
		info.name = j.value("name", "Untitled");
		info.version = j.value("version", "0.1.0");
		info.startupScene = j.value("startupScene", "");
		info.rootDirectory = projFilePath.parent_path();
		info.projectFilePath = projFilePath;

		if (j.contains("scenes"))
			info.scenes = j["scenes"].get<std::vector<std::string>>();
		if (j.contains("assetDirectories"))
			info.assetDirectories = j["assetDirectories"].get<std::vector<std::string>>();

		return std::make_shared<Project>(info);
	}

	std::shared_ptr<Project> Project::create(const std::filesystem::path& directory, const std::string& name)
	{
		std::filesystem::path projDir = directory / name;
		std::filesystem::path projFile = projDir / (name + ".fufuproj");

		std::filesystem::create_directories(projDir);

		ProjectInfo info;
		info.name = name;
		info.rootDirectory = projDir;
		info.projectFilePath = projFile;
		info.startupScene = "Scenes/Main.fufuscene";
		info.scenes = { "Scenes/Main.fufuscene" };
		info.assetDirectories = { "Assets" };

		auto project = std::make_shared<Project>(info);
		project->save();
		return project;
	}

}