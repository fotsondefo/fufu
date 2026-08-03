#pragma once

#include "ProjectInfo.h"
#include "WorldSettings.h"
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

		WorldSettings&       getWorldSettings()       { return m_WorldSettings; }
		const WorldSettings& getWorldSettings() const { return m_WorldSettings; }

		const std::string&            getName()    const { return m_Info.name; }
		const std::filesystem::path&  getRootDir() const { return m_Info.rootDirectory; }

		void save() const;
		static std::shared_ptr<Project> load(const std::filesystem::path& projFilePath);
		static std::shared_ptr<Project> create(const std::filesystem::path& directory, const std::string& name);

		// Adds `relativePath` (e.g. "Scenes/Main.fufuscene") to the project's
		// known scene list if not already present, and immediately persists the
		// .fufuproj — without this, a scene saved to disk was never reloaded on
		// the next launch (nothing updated this list after Project::create()).
		void registerScene(const std::string& relativePath);

		// Saves ALL currently loaded scenes to disk (under
		// scenesDir()/<name>.fufuscene) and registers them in the manifest.
		// Called on project close: without this, a scene created via
		// "+ New Scene" but never manually saved would simply vanish on the
		// next launch.
		void saveAllLoadedScenes();

	private:
		ProjectInfo                   m_Info;
		WorldSettings                 m_WorldSettings;
		std::unique_ptr<AssetManager> m_AssetManager;
		std::unique_ptr<SceneManager> m_SceneManager;
	};

}