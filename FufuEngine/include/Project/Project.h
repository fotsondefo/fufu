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

		// Ajoute `relativePath` (ex: "Scenes/Main.fufuscene") à la liste des
		// scènes connues du projet si elle n'y est pas déjà, et persiste
		// immédiatement le .fufuproj — sans ça une scène sauvegardée sur disque
		// n'était jamais rechargée au lancement suivant (rien ne mettait à jour
		// cette liste après Project::create()).
		void registerScene(const std::string& relativePath);

		// Sauvegarde sur disque TOUTES les scènes actuellement chargées (sous
		// scenesDir()/<nom>.fufuscene) et les enregistre dans le manifeste.
		// Appelé à la fermeture du projet : sans ça, une scène créée via
		// "+ New Scene" mais jamais sauvegardée manuellement disparaissait
		// purement et simplement au prochain lancement.
		void saveAllLoadedScenes();

	private:
		ProjectInfo                  m_Info;
		std::unique_ptr<AssetManager> m_AssetManager;
		std::unique_ptr<SceneManager> m_SceneManager;
	};

}