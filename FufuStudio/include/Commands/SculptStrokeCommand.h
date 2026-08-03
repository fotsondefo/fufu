#pragma once

#include "ICommand.h"
#include <Project/Assets/MeshAsset.h>
#include <Project/Assets/MeshExporter.h>
#include <Application/Application.h>
#include <filesystem>

namespace FufuStudio
{
	// A sculpt brush stroke (potentially dozens of frames of continuous
	// displacement) captured as ONE single before/after transition —
	// same logic as the gizmo drag: the live movement pushes nothing into
	// the history, only releasing the mouse does.
	class SculptStrokeCommand : public ICommand
	{
	public:
		SculptStrokeCommand(std::filesystem::path meshPath, std::size_t subMeshIndex,
			Fufu::SubMesh before, Fufu::SubMesh after)
			: m_MeshPath(std::move(meshPath)), m_SubMeshIndex(subMeshIndex)
			, m_Before(std::move(before)), m_After(std::move(after)) {}

		void execute() override { apply(m_After); }
		void undo() override { apply(m_Before); }
		const char* getName() const override { return "Sculpt Stroke"; }

	private:
		void apply(const Fufu::SubMesh& mesh)
		{
			auto& pm = Fufu::Application::get().getProjectManager();
			if (!pm.hasProject()) return;

			auto asset = pm.getCurrentProject().getAssetManager().getMesh(m_MeshPath);
			if (!asset) return;

			auto& subMeshes = asset->getSubMeshesMutable();
			if (m_SubMeshIndex >= subMeshes.size()) return;

			subMeshes[m_SubMeshIndex] = mesh;
			asset->invalidateLODs(); // LOD0 has changed, the generated LODs are stale
			Fufu::MeshExporter::writeObj(m_MeshPath, mesh);

			// See ExtrudeFaceCommand::markActiveSceneDirty for the rationale
			// (asset geometry, no ECS component to observe automatically).
			if (auto scene = pm.getCurrentProject().getSceneManager().getActiveScene())
				scene->markDirty();

			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		std::filesystem::path m_MeshPath;
		std::size_t m_SubMeshIndex;
		Fufu::SubMesh m_Before;
		Fufu::SubMesh m_After;
	};
}
