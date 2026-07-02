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

	bool SceneIO::newScene(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject())
		{
			FUFU_WARN("SceneIO: no project open");
			return false;
		}

		auto& sm = pm.getCurrentProject().getSceneManager();
		auto scene = sm.newScene("Untitled");
		sm.setActiveScene("Untitled");

		state.selectedEntity = Fufu::Entity{};
		if (state.commandHistory) state.commandHistory->clear();
		Fufu::Application::get().getRenderer().resetAccumulation();

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

		return pm.getCurrentProject().getSceneManager().saveScene(scene, path);
	}

	bool SceneIO::saveSceneAs(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject()) 
			return false;

		auto scene = state.getActiveScene();
		if (!scene) 
			return false;

		NFD::Guard nfdGuard;
		NFD::UniquePath outPath;

		nfdresult_t result = NFD::SaveDialog(
			outPath,
			k_Filters,
			static_cast<nfdfiltersize_t>(k_FilterCount),
			pm.getCurrentProject().getInfo().scenesDir().string().c_str(),
			(scene->getName() + ".fufuscene").c_str()
		);

		if (result == NFD_OKAY)
		{
			std::filesystem::path path(outPath.get());
			if (path.extension().empty()) path += ".fufuscene";
			
			return pm.getCurrentProject().getSceneManager().saveScene(scene, path);
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
			auto& sm = pm.getCurrentProject().getSceneManager();
			auto  scene = sm.loadScene(path);
			if (!scene) return false;

			sm.setActiveScene(scene->getName());
			state.selectedEntity = Fufu::Entity{};
			if (state.commandHistory) state.commandHistory->clear();
			Fufu::Application::get().getRenderer().resetAccumulation();
			return true;
		}

		return false;
	}

}