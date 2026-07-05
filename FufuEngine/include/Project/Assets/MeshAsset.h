#pragma once

#include "Asset.h"
#include "Renderer/GPUBuffers.h"
#include "Renderer/BVH.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <unordered_map>

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

	// BLAS d'un niveau de LOD : géométrie (espace local, tous les sous-maillages
	// concaténés) + BVH construit dessus. Mis en cache par MeshAsset (voir
	// getOrBuildBLAS) — construit une seule fois, pas à chaque frame où le
	// mesh est référencé par une scène.
	// La géométrie est déjà séparée en deux buffers au niveau du cache : positions
	// (lues pendant la traversée du BVH) et attributs (lus une seule fois, sur
	// le hit final — voir GPUTrianglePosition/GPUTriangleAttribute).
	struct MeshBLAS
	{
		std::vector<GPUTrianglePosition>  positions;
		std::vector<GPUTriangleAttribute> attributes;
		std::vector<GPUBVHNode>           nodes;
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
		// Invalide aussi le cache de BLAS (getOrBuildBLAS) puisqu'il dérive
		// directement des sous-maillages.
		void invalidateLODs()
		{
			m_LODLevels.clear();
			m_LODsGenerated = false;
			m_BLASCache.clear();
			++m_BLASVersion;
		}

		// BLAS (triangles + BVH) du niveau de LOD demandé, construit une seule
		// fois puis mis en cache — c'est CE cache, pas GPUScene, qui décide
		// quand un BVH doit être reconstruit. Bouger une entité qui référence
		// ce mesh ne le touche jamais : seule invalidateLODs() (sculpt,
		// extrude, changement de mesh) le fait.
		const MeshBLAS& getOrBuildBLAS(int lod) const;

		// Incrémenté à chaque fois qu'un BLAS (n'importe quel LOD) est
		// (re)construit — sert de signature à GPUScene pour savoir si ses
		// buffers GPU concaténés sont encore à jour, sans comparer le contenu.
		uint64_t getBLASVersion() const { return m_BLASVersion; }

	private:
		void ensureLODsGenerated() const;

		std::vector<SubMesh> m_SubMeshes; // LOD0

		mutable std::vector<std::vector<SubMesh>> m_LODLevels; // LOD1, LOD2, ...
		mutable bool m_LODsGenerated = false;

		mutable std::unordered_map<int, MeshBLAS> m_BLASCache; // par niveau de LOD
		mutable uint64_t m_BLASVersion = 0;

		friend class AssetManager;
	};
}
