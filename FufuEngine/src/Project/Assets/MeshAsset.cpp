#include "depch.h"
#include "Project/Assets/MeshAsset.h"
#include "Project/Assets/MeshSimplifier.h"
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace Fufu
{
	const std::vector<SubMesh>& MeshAsset::getLODSubMeshes(int level) const
	{
		if (level <= 0)
			return m_SubMeshes;

		ensureLODsGenerated();

		if (m_LODLevels.empty())
			return m_SubMeshes;

		std::size_t idx = static_cast<std::size_t>(level - 1);
		if (idx >= m_LODLevels.size())
			idx = m_LODLevels.size() - 1; // retombe sur le LOD le plus simplifié disponible

		return m_LODLevels[idx];
	}

	const MeshBLAS& MeshAsset::getOrBuildBLAS(int lod) const
	{
		auto it = m_BLASCache.find(lod);
		if (it != m_BLASCache.end())
			return it->second;

		// Format combiné temporaire : BVHBuilder::build a besoin d'un seul
		// vecteur à réordonner en place (centroïdes/bounds calculés dessus).
		// Éclaté en positions/attributs juste après (voir splitTriangleBuffers).
		std::vector<GPUTriangle> combined;
		for (const SubMesh& sub : getLODSubMeshes(lod))
		{
			for (std::size_t i = 0; i + 2 < sub.indices.size(); i += 3)
			{
				const Vertex& a = sub.vertices[sub.indices[i]];
				const Vertex& b = sub.vertices[sub.indices[i + 1]];
				const Vertex& c = sub.vertices[sub.indices[i + 2]];

				GPUTriangle tri{};
				tri.v0 = glm::vec4(a.position, 0.f);
				tri.v1 = glm::vec4(b.position, 0.f);
				tri.v2 = glm::vec4(c.position, 0.f);
				tri.n0 = glm::vec4(a.normal, 0.f);
				tri.n1 = glm::vec4(b.normal, 0.f);
				tri.n2 = glm::vec4(c.normal, 0.f);
				tri.uv0 = a.uv;
				tri.uv1 = b.uv;
				tri.uv2 = c.uv;
				tri.materialIndex = 0; // le matériau vient de l'instance, pas du BLAS partagé
				combined.push_back(tri);
			}
		}

		MeshBLAS blas;
		if (!combined.empty())
		{
			// Réordonne `combined` en place (feuilles contiguës) : on éclate
			// APRÈS pour que positions/attributs partagent le même ordre/index.
			blas.nodes = BVHBuilder::build(combined);
			splitTriangleBuffers(combined, blas.positions, blas.attributes);
		}

		++m_BLASVersion;
		auto result = m_BLASCache.emplace(lod, std::move(blas));
		return result.first->second;
	}

	void MeshAsset::ensureLODsGenerated() const
	{
		if (m_LODsGenerated) return;
		m_LODsGenerated = true;
		m_LODLevels.clear();

		if (m_SubMeshes.empty()) return;

		glm::vec3 bmin(1e30f, 1e30f, 1e30f);
		glm::vec3 bmax(-1e30f, -1e30f, -1e30f);
		for (const SubMesh& sub : m_SubMeshes)
		{
			for (const Vertex& v : sub.vertices)
			{
				bmin = glm::min(bmin, v.position);
				bmax = glm::max(bmax, v.position);
			}
		}

		float diagonal = glm::length(bmax - bmin);
		if (diagonal <= 0.f) return;

		// Facteurs empiriques : taille de cellule = diagonale / facteur.
		// Plus le facteur est petit, plus la simplification est agressive.
		static const float k_CellFactors[k_ExtraLODLevels] = { 40.f, 15.f };

		for (float factor : k_CellFactors)
		{
			float cellSize = diagonal / factor;

			std::vector<SubMesh> level;
			level.reserve(m_SubMeshes.size());
			for (const SubMesh& sub : m_SubMeshes)
				level.push_back(MeshSimplifier::simplify(sub, cellSize));

			m_LODLevels.push_back(std::move(level));
		}
	}
}
