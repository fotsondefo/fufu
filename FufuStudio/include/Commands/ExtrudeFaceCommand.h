#pragma once

#include "ICommand.h"
#include <Project/Assets/MeshAsset.h>
#include <Project/Assets/MeshExporter.h>
#include <Application/Application.h>
#include <filesystem>
#include <glm/glm.hpp>

namespace FufuStudio
{
	// Extrude une face (triangle) unique d'un mesh : duplique ses 3 sommets,
	// les décale le long de la normale moyenne de la face, et referme les 3
	// parois latérales entre l'ancien contour et le nouveau. "Face" = triangle
	// du buffer d'indices ici, pas un n-gone fusionné (limitation assumée).
	//
	// Undo/redo par snapshot complet du SubMesh avant/après : simple et sûr,
	// coût négligeable pour les tailles de mesh actuelles (primitives).
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

			Fufu::SubMesh mesh = m_Before; // repartir d'un état propre (nécessaire au redo)

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

			// Nouvelle face, à la position extrudée (garde la normale d'origine)
			uint32_t na = push(va.position + offset, va.normal, va.uv);
			uint32_t nb = push(vb.position + offset, vb.normal, vb.uv);
			uint32_t nc = push(vc.position + offset, vc.normal, vc.uv);

			mesh.indices[i0]     = na;
			mesh.indices[i0 + 1] = nb;
			mesh.indices[i0 + 2] = nc;

			// Parois latérales (sommets dupliqués pour un éclairage plat correct :
			// la paroi a sa propre normale, différente de celle du plafond/plancher)
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

			Fufu::MeshExporter::writeObj(m_MeshPath, mesh);
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		void undo() override
		{
			auto asset = getAsset();
			if (!asset) return;

			auto& subMeshes = asset->getSubMeshesMutable();
			if (m_SubMeshIndex >= subMeshes.size()) return;

			subMeshes[m_SubMeshIndex] = m_Before;
			Fufu::MeshExporter::writeObj(m_MeshPath, m_Before);
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		void redo() override
		{
			auto asset = getAsset();
			if (!asset) return;

			auto& subMeshes = asset->getSubMeshesMutable();
			if (m_SubMeshIndex >= subMeshes.size()) return;

			subMeshes[m_SubMeshIndex] = m_After;
			Fufu::MeshExporter::writeObj(m_MeshPath, m_After);
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

		std::filesystem::path m_MeshPath;
		std::size_t m_SubMeshIndex;
		std::size_t m_FaceIndex;
		float m_Distance;
		Fufu::SubMesh m_Before;
		Fufu::SubMesh m_After;
	};
}
