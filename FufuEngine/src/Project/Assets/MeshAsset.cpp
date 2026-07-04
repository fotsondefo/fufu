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
