#include "depch.h"
#include "Project/Project.h"
#include <nlohmann/json.hpp>
#include <fstream>

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
		std::filesystem::create_directories(m_Info.configDir() / "editor");

		// Scan existing assets
		m_AssetManager->scanDirectory();

		// Load StartupScene
		if (!m_Info.startupScene.empty())
		{
			std::filesystem::path scenePath =
				m_Info.rootDirectory / m_Info.startupScene;

			if (std::filesystem::exists(scenePath))
			{
				auto scene = m_SceneManager->loadScene(scenePath);
				if (scene)
					m_SceneManager->setActiveScene(scene->getName());
			}
			else
			{
				// Create default scene
				auto scene = m_SceneManager->newScene("Main");
				m_SceneManager->setActiveScene("Main");
			}
		}
		else
		{
			auto scene = m_SceneManager->newScene("Main");
			m_SceneManager->setActiveScene("Main");
		}

		FUFU_INFO("Project '{}' initialized", m_Info.name);
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