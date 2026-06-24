#include "SceneIO.h"
#include "Project/SceneSerializer.h"
#include "Application/Application.h"
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
		state.activeScene = std::make_shared<Fufu::Scene>("Untitled");
		state.currentPath = "";
		state.selectedEntity = Fufu::Entity{};
		Fufu::Application::get().getRenderer().resetAccumulation();
		FUFU_INFO("New scene created");
		return true;
	}

	bool SceneIO::saveScene(EditorState& state)
	{
		if (state.currentPath.empty())
			return saveSceneAs(state);

		return saveToPath(state, state.currentPath);
	}

	bool SceneIO::saveSceneAs(EditorState& state)
	{
		NFD::Guard nfdGuard;
		NFD::UniquePath outPath;

		nfdresult_t result = NFD::SaveDialog(
			outPath,
			k_Filters,
			static_cast<nfdfiltersize_t>(k_FilterCount),
			nullptr,                // default repository
			"scene.fufuscene"       // Default name
		);

		if (result == NFD_OKAY)
		{
			std::filesystem::path path(outPath.get());

			// Add the extension if it's not present in the filepath
			if (path.extension().empty())
				path += ".fufuscene";

			return saveToPath(state, path);
		}
		else if (result == NFD_CANCEL)
		{
			FUFU_INFO("Save cancelled by user");
		}
		else
		{
			FUFU_ERROR("NFD Save error: {}", NFD::GetError());
		}

		return false;
	}

	bool SceneIO::openScene(EditorState& state)
	{
		NFD::Guard nfdGuard;
		NFD::UniquePath inPath;

		nfdresult_t result = NFD::OpenDialog(
			inPath,
			k_Filters,
			k_FilterCount,
			nullptr
		);

		if (result == NFD_OKAY)
		{
			std::filesystem::path path(inPath.get());

			auto newScene = std::make_shared<Fufu::Scene>();
			Fufu::SceneSerializer serializer(newScene.get());

			if (!serializer.deserialize(path))
			{
				FUFU_ERROR("Failed to load scene: {}", path.string());
				return false;
			}

			state.activeScene = newScene;
			state.currentPath = path.string();
			state.selectedEntity = Fufu::Entity{};
			Fufu::Application::get().getRenderer().resetAccumulation();
			FUFU_INFO("Scene loaded: {}", path.string());
			return true;
		}
		else if (result == NFD_CANCEL)
		{
			FUFU_INFO("Open cancelled by user");
		}
		else
		{
			FUFU_ERROR("NFD Open error: {}", NFD::GetError());
		}

		return false;
	}

	bool SceneIO::saveToPath(EditorState& state, const std::filesystem::path& path)
	{
		if (!state.activeScene)
		{
			FUFU_ERROR("No active scene to save");
			return false;
		}

		// Create parent folder if necessary
		std::filesystem::create_directories(path.parent_path());

		Fufu::SceneSerializer serializer(state.activeScene.get());
		serializer.serialize(path);

		state.currentPath = path.string();
		FUFU_INFO("Scene saved: {}", path.string());
		return true;
	}

}