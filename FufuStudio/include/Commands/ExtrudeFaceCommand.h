#pragma once

#include "ICommand.h"
#include <Project/Assets/MeshAsset.h>
#include <Project/Assets/MeshExporter.h>
#include <Application/Application.h>
#include <filesystem>
#include <glm/glm.hpp>

namespace FufuStudio
{
	// Extrudes a single face (triangle) of a mesh: duplicates its 3 vertices,
	// offsets them along the average normal of the face, and closes the 3
	// lateral walls between the old outline and the new one. "Face" = triangle
	// of the index buffer here, not a merged n-gon (assumed limitation).
	//
	// Undo/redo via full SubMesh snapshot before/after: simple and safe,
	// negligible cost for the current mesh sizes (primitives).
	class ExtrudeFaceCommand : public ICommand
	{
	public:
		ExtrudeFaceCommand(std::filesystem::path meshPath, std::size_t subMeshIndex,
			std::size_t faceIndex, float distance)
			: m_MeshPath(std::move(meshPath)), m_SubMeshIndex(subMeshIndex)
			, m_FaceIndex(faceIndex), m_Distance(distance)
		{
			if (auto asset = getAsset())
			{
				auto& subMeshes = asset->getSubMeshesMutable();
				if (m_SubMeshIndex < subMeshes.size())
					m_Before = subMeshes[m_SubMeshIndex];
			}
		}

		void execute() override
		{
			auto asset = getAsset();
			if (!asset) return;

			auto& subMeshes = asset->getSubMeshesMutable();
			if (m_SubMeshIndex >= subMeshes.size()) return;

			Fufu::SubMesh mesh = m_Before; // start from a clean state (required for redo)

			std::size_t i0 = m_FaceIndex * 3;
			if (i0 + 2 >= mesh.indices.size()) return;

			uint32_t ia = mesh.indices[i0];
			uint32_t ib = mesh.indices[i0 + 1];
			uint32_t ic = mesh.indices[i0 + 2];

			Fufu::Vertex va = mesh.vertices[ia];
			Fufu::Vertex vb = mesh.vertices[ib];
			Fufu::Vertex vc = mesh.vertices[ic];

			glm::vec3 offset = glm::normalize(va.normal + vb.normal + vc.normal) * m_Distance;

			auto push = [&](const glm::vec3& pos, const glm::vec3& normal, const glm::vec2& uv)
			{
				Fufu::Vertex v;
				v.position = pos;
				v.normal = normal;
				v.uv = uv;
				v.tangent = glm::vec3(0.f);
				uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
				mesh.vertices.push_back(v);
				return idx;
			};

			// New face, at the extruded position (keeps the original normal)
			uint32_t na = push(va.position + offset, va.normal, va.uv);
			uint32_t nb = push(vb.position + offset, vb.normal, vb.uv);
			uint32_t nc = push(vc.position + offset, vc.normal, vc.uv);

			mesh.indices[i0]     = na;
			mesh.indices[i0 + 1] = nb;
			mesh.indices[i0 + 2] = nc;

			// Lateral walls (vertices duplicated for correct flat shading:
			// the wall has its own normal, different from that of the top/bottom face)
			auto addWall = [&](const Fufu::Vertex& oldA, const Fufu::Vertex& oldB)
			{
				glm::vec3 wallNormal = glm::normalize(glm::cross(oldB.position - oldA.position, offset));

				uint32_t wOldA = push(oldA.position, wallNormal, oldA.uv);
				uint32_t wOldB = push(oldB.position, wallNormal, oldB.uv);
				uint32_t wNewA = push(oldA.position + offset, wallNormal, oldA.uv);
				uint32_t wNewB = push(oldB.position + offset, wallNormal, oldB.uv);

				mesh.indices.push_back(wOldA);
				mesh.indices.push_back(wOldB);
				mesh.indices.push_back(wNewB);

				mesh.indices.push_back(wOldA);
				mesh.indices.push_back(wNewB);
				mesh.indices.push_back(wNewA);
			};

			addWall(va, vb);
			addWall(vb, vc);
			addWall(vc, va);

			subMeshes[m_SubMeshIndex] = mesh;
			m_After = mesh;

			asset->invalidateLODs(); // LOD0 has changed, the generated LODs are stale
			Fufu::MeshExporter::writeObj(m_MeshPath, mesh);
			markActiveSceneDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		void undo() override
		{
			auto asset = getAsset();
			if (!asset) return;

			auto& subMeshes = asset->getSubMeshesMutable();
			if (m_SubMeshIndex >= subMeshes.size()) return;

			subMeshes[m_SubMeshIndex] = m_Before;
			asset->invalidateLODs();
			Fufu::MeshExporter::writeObj(m_MeshPath, m_Before);
			markActiveSceneDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		void redo() override
		{
			auto asset = getAsset();
			if (!asset) return;

			auto& subMeshes = asset->getSubMeshesMutable();
			if (m_SubMeshIndex >= subMeshes.size()) return;

			subMeshes[m_SubMeshIndex] = m_After;
			asset->invalidateLODs();
			Fufu::MeshExporter::writeObj(m_MeshPath, m_After);
			markActiveSceneDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		const char* getName() const override { return "Extrude Face"; }

	private:
		std::shared_ptr<Fufu::MeshAsset> getAsset() const
		{
			auto& pm = Fufu::Application::get().getProjectManager();
			if (!pm.hasProject()) return nullptr;
			return pm.getCurrentProject().getAssetManager().getMesh(m_MeshPath);
		}

		// The asset geometry has changed, not an ECS component: nothing detects
		// this automatically. We don't know here which entity/entities reference
		// this mesh, so we mark the active scene dirty (practical case: Extrude
		// only operates on the selected entity in the active scene) — another
		// loaded scene referencing the same mesh will refresh itself as soon as
		// it becomes active again (identity change detected by Renderer::sceneNeedsUpdate).
		static void markActiveSceneDirty()
		{
			auto& pm = Fufu::Application::get().getProjectManager();
			if (!pm.hasProject()) return;

			if (auto scene = pm.getCurrentProject().getSceneManager().getActiveScene())
				scene->markDirty();
		}

		std::filesystem::path m_MeshPath;
		std::size_t m_SubMeshIndex;
		std::size_t m_FaceIndex;
		float m_Distance;
		Fufu::SubMesh m_Before;
		Fufu::SubMesh m_After;
	};
}
