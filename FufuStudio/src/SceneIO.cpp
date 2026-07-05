#include "SceneIO.h"
#include "Application/Application.h"
#include "Commands/CommandHistory.h"
#include <nfd.hpp>

namespace FufuStudio 
{

	static const nfdfilteritem_t k_Filters[] = {
		{ "Fufu Scene", "fufuscene" },
		{ "JSON",       "json"      }
	};
	static const int k_FilterCount = 2;

	// Enregistre `path` dans le manifeste du projet (.fufuproj) si elle est
	// bien à l'intérieur du dossier racine — une Save As en dehors du projet
	// (export ponctuel) ne doit pas polluer la liste des scènes rechargées
	// automatiquement à l'ouverture du projet.
	static void registerSceneIfInsideProject(Fufu::Project& proj, const std::filesystem::path& path)
	{
		std::filesystem::path rel = std::filesystem::relative(path, proj.getRootDir());
		if (rel.empty() || rel.generic_string().rfind("..", 0) == 0)
			return;

		proj.registerScene(rel.generic_string());
	}

	bool SceneIO::newScene(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject())
		{
			FUFU_WARN("SceneIO: no project open");
			return false;
		}

		auto& proj = pm.getCurrentProject();
		auto& sm = proj.getSceneManager();
		auto scene = sm.newScene("Untitled");
		sm.setActiveScene("Untitled");

		state.selection.clear();
		if (state.commandHistory) state.commandHistory->clear();
		state.syncToActiveScene();
		Fufu::Application::get().getRenderer().resetAccumulation();

		// Sauvegarde immédiate : sans ça la scène ne survivrait pas à une
		// fermeture de l'appli tant qu'on n'a pas fait Save explicitement.
		std::filesystem::path path = proj.getInfo().scenesDir() / "Untitled.fufuscene";
		if (sm.saveScene(scene, path))
			registerSceneIfInsideProject(proj, path);

		return true;
	}

	bool SceneIO::saveScene(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject()) return false;

		auto scene = state.getActiveScene();
		if (!scene) return false;

		auto& proj = pm.getCurrentProject();
		std::filesystem::path path = proj.getInfo().scenesDir() / (scene->getName() + ".fufuscene");

		if (!proj.getSceneManager().saveScene(scene, path))
			return false;

		registerSceneIfInsideProject(proj, path);
		return true;
	}

	bool SceneIO::saveSceneAs(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject())
			return false;

		auto scene = state.getActiveScene();
		if (!scene)
			return false;

		auto& proj = pm.getCurrentProject();

		NFD::Guard nfdGuard;
		NFD::UniquePath outPath;

		nfdresult_t result = NFD::SaveDialog(
			outPath,
			k_Filters,
			static_cast<nfdfiltersize_t>(k_FilterCount),
			proj.getInfo().scenesDir().string().c_str(),
			(scene->getName() + ".fufuscene").c_str()
		);

		if (result == NFD_OKAY)
		{
			std::filesystem::path path(outPath.get());
			if (path.extension().empty()) path += ".fufuscene";

			if (!proj.getSceneManager().saveScene(scene, path))
				return false;

			registerSceneIfInsideProject(proj, path);
			return true;
		}

		return false;
	}

	bool SceneIO::openScene(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject()) return false;

		NFD::Guard nfdGuard;
		NFD::UniquePath inPath;

		nfdresult_t result = NFD::OpenDialog(
			inPath,
			k_Filters,
			static_cast<nfdfiltersize_t>(k_FilterCount),
			pm.getCurrentProject().getInfo().scenesDir().string().c_str()
		);

		if (result == NFD_OKAY)
		{
			std::filesystem::path path(inPath.get());
			auto& proj = pm.getCurrentProject();
			auto& sm = proj.getSceneManager();
			auto  scene = sm.loadScene(path);
			if (!scene) return false;

			sm.setActiveScene(scene->getName());
			state.selection.clear();
			if (state.commandHistory) state.commandHistory->clear();
			state.syncToActiveScene();
			Fufu::Application::get().getRenderer().resetAccumulation();

			// Un fichier de scène déjà présent dans le projet (copié à la main,
			// ou déplacé) mais absent du manifeste : l'ouvrir explicitement
			// l'enregistre pour qu'il revienne au prochain lancement.
			registerSceneIfInsideProject(proj, path);
			return true;
		}

		return false;
	}

}