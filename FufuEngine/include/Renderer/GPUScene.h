#pragma once

#include "GPUBuffers.h"
#include "BVH.h"
#include "Project/Scene/Scene.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Fufu
{
	class AssetManager;

	// Empaquette une Fufu::Scene en géométrie/matériaux/lumières GPU : BLAS
	// (triangles + BVH par mesh unique en espace local, partagé entre
	// instances — c'est ça l'instancing) et TLAS (BVH sur les boîtes
	// englobantes monde des instances).
	//
	// Deux catégories de données GPU, mises à jour à des rythmes très
	// différents :
	//  - géométrie (triangles/nœuds BLAS) : STATIC_DRAW, reconstruite et
	//    ré-uploadée seulement quand l'ENSEMBLE des (mesh, LOD) réellement
	//    référencés par la scène change (nouveau mesh, sculpt/extrude,
	//    changement de LOD par distance) — jamais quand une entité bouge.
	//    Le BVH de chaque mesh lui-même n'est construit qu'une fois par
	//    MeshAsset (voir MeshAsset::getOrBuildBLAS), pas ici.
	//  - instances/matériaux/lumières/TLAS : DYNAMIC_DRAW, reconstruites à
	//    chaque upload() (coût proportionnel au nombre d'entités/lumières,
	//    pas au nombre de triangles — négligeable même sur un mesh à 50k+
	//    triangles).
	class GPUScene
	{
	public:
		void init();
		void shutdown();

		// Re-synchronise la géométrie/matériaux/lumières GPU avec la scène.
		// Voir le commentaire de classe pour ce qui est réellement reconstruit.
		void upload(Scene& scene);

		// Bind les SSBOs aux binding points attendus par le compute shader :
		// 2=positions, 3=materials, 6=blasNodes, 7=instances, 8=tlasNodes,
		// 9=lights, 10=attributs.
		void bind() const;

		// triangleCount côté shader est réutilisé comme "nombre d'instances"
		// (garde anti-TLAS-vide) — voir GPUFrameData::triangleCount.
		int getInstanceCount() const { return static_cast<int>(m_Instances.size()); }
		int getMaterialCount() const { return static_cast<int>(m_Materials.size()); }
		int getLightCount()    const { return static_cast<int>(m_Lights.size()); }

		// Triangles UNIQUES dans les BLAS concaténés — pas multiplié par le
		// nombre d'instances (voir ProfilerPanel : "triangles totaux" côté
		// scène GPU, ce n'est pas la même notion que le nombre de triangles
		// effectivement traversés par un rayon).
		int getTriangleCount() const { return static_cast<int>(m_TrianglePositions.size()); }

		// Textures albedo référencées par les matériaux de cette frame, dans
		// l'ordre attendu par u_MaterialTextures[] côté shader (index i ici <->
		// GPUMaterial::albedoTexIdx == i pour les matériaux qui l'utilisent).
		const std::vector<uint32_t>& getMaterialTextures() const { return m_ActiveMaterialTextures; }

		// Nombre de triangles par instance (même ordre que getInstances()).
		// Utilisé par GBufferPass/ForwardPass pour le draw call SSBO-based.
		const std::vector<int>& getInstanceTriCounts() const { return m_InstanceTriCounts; }

		// Accès en lecture aux données CPU pour les outils de visualisation
		// (FufuLab). Les vecteurs restent valides entre deux appels à upload().
		const std::vector<GPUBVHNode>&           getBLASNodes()          const { return m_BLASNodes; }
		const std::vector<GPUBVHNode>&           getTLASNodes()          const { return m_TLASNodes; }
		const std::vector<GPUInstance>&          getInstances()          const { return m_Instances; }
		const std::vector<GPUTrianglePosition>&  getTrianglePositions()  const { return m_TrianglePositions; }
		const std::vector<GPUTriangleAttribute>& getTriangleAttributes() const { return m_TriangleAttributes; }

	private:
		// Résout (et met en cache, voir m_MaterialTextureCache) la texture GL
		// référencée par un MaterialComponent::albedoTexID, et lui assigne un
		// slot pour CETTE frame (0 si déjà bindée pour un autre matériau
		// identique). Renvoie -1 si pas de texture, introuvable, ou limite
		// kMaxMaterialTextures atteinte (auquel cas le matériau retombe sur
		// son albedo uni).
		int resolveAlbedoTexture(uint64_t textureUUID, AssetManager& assetManager,
			std::unordered_map<std::string, int>& frameSlots);

		// Offsets dans les buffers concaténés m_TrianglePositions/m_BLASNodes
		// (m_TriangleAttributes partage les mêmes indices), plus une signature
		// ("sourceVersion") permettant de détecter que la source
		// (MeshAsset::getBLASVersion, ou le hash des paramètres groom) a changé
		// depuis la dernière concaténation, sans comparer le contenu.
		struct BLASRef
		{
			int nodeOffset;
			int triOffset;
			int triCount;       // nombre de triangles dans ce BLAS
			uint64_t sourceVersion;
		};

		// Persistant entre deux appels à upload() — c'est ce qui permet de
		// sauter la reconstruction quand rien n'a changé côté géométrie.
		std::unordered_map<std::string, BLASRef> m_BLASCache;

		// Cache persistant chemin -> texture GL, construit une seule fois par
		// texture référencée (jamais re-uploadée tant que le fichier ne
		// change pas). m_ActiveMaterialTextures est l'ordre de bind ACTUEL
		// (recalculé à chaque upload() puisque l'ensemble des matériaux
		// référencés peut changer d'une frame à l'autre), capé à
		// kMaxMaterialTextures.
		std::unordered_map<std::string, uint32_t> m_MaterialTextureCache;
		std::vector<uint32_t> m_ActiveMaterialTextures;

		uint32_t m_TrianglePositionSSBO = 0;  // Intersection uniquement (voir GPUTrianglePosition)
		uint32_t m_TriangleAttributeSSBO = 0; // Normales/UV/matériau, lus une seule fois sur le hit final
		uint32_t m_MaterialSSBO = 0;
		uint32_t m_BLASNodeSSBO = 0;  // BLAS : nœuds BVH, concaténés par mesh unique
		uint32_t m_InstanceSSBO = 0;  // une entrée par instance (transform + réf. BLAS + matériau)
		uint32_t m_TLASNodeSSBO = 0;  // BVH de plus haut niveau, sur les boîtes des instances
		uint32_t m_LightSSBO = 0;

		std::vector<GPUTrianglePosition>  m_TrianglePositions;  // BLAS, espace local, concaténés par mesh unique
		std::vector<GPUTriangleAttribute> m_TriangleAttributes; // mêmes indices que m_TrianglePositions
		std::vector<GPUMaterial> m_Materials;
		std::vector<GPUBVHNode>  m_BLASNodes;   // BLAS, concaténés par mesh unique
		std::vector<GPUInstance> m_Instances;
		std::vector<int>         m_InstanceTriCounts; // mêmes indices que m_Instances
		std::vector<GPUBVHNode>  m_TLASNodes;
		std::vector<GPULight>    m_Lights;
	};
}
