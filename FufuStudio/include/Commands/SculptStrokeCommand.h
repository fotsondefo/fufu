#pragma once

#include "ICommand.h"
#include <Project/Assets/MeshAsset.h>
#include <Project/Assets/MeshExporter.h>
#include <Application/Application.h>
#include <filesystem>

namespace FufuStudio
{
	// Un coup de pinceau de sculpt (potentiellement des dizaines de frames de
	// déplacement continu) capturé comme UNE seule transition avant/après —
	// même logique que le drag du gizmo : le déplacement live ne pousse rien
	// dans l'historique, seul le relâchement de la souris le fait.
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
			Fufu::MeshExporter::writeObj(m_MeshPath, mesh);
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		std::filesystem::path m_MeshPath;
		std::size_t m_SubMeshIndex;
		Fufu::SubMesh m_Before;
		Fufu::SubMesh m_After;
	};
}
