#include "Helpers/PrimitiveFactory.h"
#include <Project/Components.h>
#include <Project/Assets/MeshExporter.h>
#include <Application/Application.h>
#include "Commands/CommandHistory.h"
#include "Commands/EntityCommands.h"

namespace FufuStudio
{
	static std::filesystem::path defaultPrimitivesDir()
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		return pm.hasProject()
			? pm.getCurrentProject().getInfo().assetsDir() / "Meshes" / "Primitives"
			: std::filesystem::current_path();
	}

	// Finds a free filename in `dir` (Cube.obj, Cube_1.obj, ...)
	static std::filesystem::path uniqueMeshFilePath(const std::filesystem::path& dir, const std::string& baseName)
	{
		std::filesystem::create_directories(dir);

		std::filesystem::path candidate = dir / (baseName + ".obj");
		int suffix = 1;
		while (std::filesystem::exists(candidate))
		{
			candidate = dir / (baseName + "_" + std::to_string(suffix) + ".obj");
			++suffix;
		}
		return candidate;
	}

	void createPrimitiveEntity(EditorState& state, const std::shared_ptr<Fufu::Scene>& scene,
		const std::string& name, const Fufu::SubMesh& mesh)
	{
		// The generated file is not deleted on undo (same logic as
		// "Create Prefab": file side-effects are not undone,
		// only the scene state is).
		std::filesystem::path path = uniqueMeshFilePath(defaultPrimitivesDir(), name);
		if (!Fufu::MeshExporter::writeObj(path, mesh))
			return;

		auto* cmd = state.commandHistory->executeCommand<EntityCreateCommand>(scene, name, Fufu::Entity{},
			[path](Fufu::Entity e)
			{
				e.addComponent<Fufu::MeshComponent>(path.string(), uint64_t(0));
				e.addComponent<Fufu::MaterialComponent>();
			});

		state.selection.select(cmd->getEntity());
	}
}
