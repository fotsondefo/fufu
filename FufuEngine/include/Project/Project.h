#pragma once

#include "ProjectInfo.h"
#include "Assets/AssetManager.h"
#include "Scene/SceneManager.h"

namespace Fufu 
{

	class Project
	{
	public:
		explicit Project(const ProjectInfo& info);
		~Project() = default;

		void init();

		// Accessors
		const ProjectInfo& getInfo()          const { return m_Info; }
		AssetManager& getAssetManager() { return *m_AssetManager; }
		SceneManager& getSceneManager() { return *m_SceneManager; }

		const std::string&            getName()    const { return m_Info.name; }
		const std::filesystem::path&  getRootDir() const { return m_Info.rootDirectory; }

		void save() const;
		static std::shared_ptr<Project> load(const std::filesystem::path& projFilePath);
		static std::shared_ptr<Project> create(const std::filesystem::path& directory, const std::string& name);

	private:
		ProjectInfo                  m_Info;
		std::unique_ptr<AssetManager> m_AssetManager;
		std::unique_ptr<SceneManager> m_SceneManager;
	};

}