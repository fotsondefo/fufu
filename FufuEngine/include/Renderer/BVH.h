#pragma once

#include "GPUBuffers.h"
#include <vector>

namespace Fufu
{
	// Nœud de BVH côté GPU (std430, 48 octets : 2×vec4 + 4×int, déjà multiple
	// de 16, pas de padding nécessaire).
	struct alignas(16) GPUBVHNode
	{
		glm::vec4 boundsMin; // w inutilisé
		glm::vec4 boundsMax; // w inutilisé
		int left;            // index du fils gauche ; -1 si feuille
		int right;           // index du fils droit ; -1 si feuille
		int firstTri;        // feuille : index de départ dans le buffer de triangles (réordonné)
		int triCount;        // feuille : nombre de triangles ; 0 si nœud interne
	};

	class BVHBuilder
	{
	public:
		// Construit un BVH par split médian sur l'axe le plus large de la boîte
		// englobante des centroïdes (pas de SAH pour l'instant — plus simple,
		// suffisant tant que la géométrie n'est pas extrême). RÉORDONNE
		// `triangles` en place pour que chaque feuille référence une plage
		// contiguë [firstTri, firstTri + triCount) — c'est pour ça qu'on prend
		// une référence non-const.
		static std::vector<GPUBVHNode> build(std::vector<GPUTriangle>& triangles, int leafSize = 4);
	};
}
