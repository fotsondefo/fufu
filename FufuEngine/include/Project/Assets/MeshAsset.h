#pragma once

#include "Asset.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Fufu
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 tangent;
	};

	struct SubMesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::string name;
	};

	class MeshAsset : public Asset
	{
	public:
		AssetType getType() const override { return AssetType::Mesh; }

		const std::vector<SubMesh>& getSubMeshes() const { return m_SubMeshes; }
		size_t getSubMeshCount() const { return m_SubMeshes.size(); }

		// Accès mutable pour les outils d'édition de mesh (voir ModelingTool /
		// SculptTool côté éditeur). Le Renderer re-upload la scène à chaque
		// frame, donc toute modification ici est visible immédiatement.
		// Toujours le niveau de détail complet (LOD0) — les outils d'édition
		// n'opèrent jamais sur un LOD simplifié.
		std::vector<SubMesh>& getSubMeshesMutable() { return m_SubMeshes; }

		// LOD : niveau 0 = détail complet (identique à getSubMeshes()). Les
		// niveaux suivants sont générés paresseusement par clustering de
		// sommets (voir MeshSimplifier) et mis en cache — le premier appel
		// avec level>0 a un coût, les suivants sont gratuits jusqu'à
		// invalidateLODs(). Un niveau demandé au-delà du dernier généré
		// retombe sur le plus simplifié disponible.
		static constexpr int k_ExtraLODLevels = 2; // en plus de LOD0
		const std::vector<SubMesh>& getLODSubMeshes(int level) const;

		// À appeler après toute édition de LOD0 (sculpt, extrude...) : les LOD
		// générés deviennent obsolètes, ils seront reconstruits à la demande.
		void invalidateLODs()
		{
			m_LODLevels.clear();
			m_LODsGenerated = false;
		}

	private:
		void ensureLODsGenerated() const;

		std::vector<SubMesh> m_SubMeshes; // LOD0

		mutable std::vector<std::vector<SubMesh>> m_LODLevels; // LOD1, LOD2, ...
		mutable bool m_LODsGenerated = false;

		friend class AssetManager;
	};
}
