#pragma once

#include "GPUBuffers.h"
#include "BVH.h"
#include "Project/Scene/Scene.h"
#include <vector>
#include <cstdint>

namespace Fufu
{
	// Empaquette une Fufu::Scene en géométrie/matériaux/lumières GPU : BLAS
	// (triangles + BVH par mesh unique en espace local, partagé entre
	// instances — c'est ça l'instancing) et TLAS (BVH sur les boîtes
	// englobantes monde des instances). Extrait de Renderer pour séparer
	// "empaqueter la scène pour le GPU" de "exécuter les passes de rendu" :
	// Renderer se contente d'appeler upload() puis bind() avant de
	// dispatcher les passes.
	class GPUScene
	{
	public:
		void init();
		void shutdown();

		// Reconstruit entièrement la géométrie/matériaux/lumières GPU à partir
		// de la scène (pas de diff incrémental pour l'instant : voir
		// Renderer::sceneNeedsUpdate).
		void upload(Scene& scene);

		// Bind les SSBOs aux binding points attendus par le compute shader :
		// 2=triangles, 3=materials, 6=blasNodes, 7=instances, 8=tlasNodes, 9=lights.
		void bind() const;

		// triangleCount côté shader est réutilisé comme "nombre d'instances"
		// (garde anti-TLAS-vide) — voir GPUFrameData::triangleCount.
		int getInstanceCount() const { return static_cast<int>(m_Instances.size()); }
		int getMaterialCount() const { return static_cast<int>(m_Materials.size()); }
		int getLightCount()    const { return static_cast<int>(m_Lights.size()); }

	private:
		uint32_t m_TriangleSSBO = 0;  // BLAS : triangles en espace local, concaténés par mesh unique
		uint32_t m_MaterialSSBO = 0;
		uint32_t m_BLASNodeSSBO = 0;  // BLAS : nœuds BVH, concaténés par mesh unique
		uint32_t m_InstanceSSBO = 0;  // une entrée par instance (transform + réf. BLAS + matériau)
		uint32_t m_TLASNodeSSBO = 0;  // BVH de plus haut niveau, sur les boîtes des instances
		uint32_t m_LightSSBO = 0;

		std::vector<GPUTriangle> m_Triangles;   // BLAS, espace local, concaténés par mesh unique
		std::vector<GPUMaterial> m_Materials;
		std::vector<GPUBVHNode>  m_BLASNodes;   // BLAS, concaténés par mesh unique
		std::vector<GPUInstance> m_Instances;
		std::vector<GPUBVHNode>  m_TLASNodes;
		std::vector<GPULight>    m_Lights;
	};
}
