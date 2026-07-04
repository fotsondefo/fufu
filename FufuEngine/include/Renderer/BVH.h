#pragma once

#include "GPUBuffers.h"
#include <vector>

namespace Fufu
{
	// Nœud de BVH côté GPU (std430, 48 octets : 2×vec4 + 4×int, déjà multiple
	// de 16, pas de padding nécessaire). Réutilisé pour le BLAS (feuilles =
	// triangles, via firstTri/triCount) ET le TLAS (feuilles = instances, les
	// mêmes champs firstTri/triCount servant alors de firstInstance/instCount).
	struct alignas(16) GPUBVHNode
	{
		glm::vec4 boundsMin; // w inutilisé
		glm::vec4 boundsMax; // w inutilisé
		int left;            // index du fils gauche ; -1 si feuille
		int right;           // index du fils droit ; -1 si feuille
		int firstTri;        // feuille : index de départ (triangles OU instances selon le buffer)
		int triCount;        // feuille : nombre d'éléments ; 0 si nœud interne
	};

	class BVHBuilder
	{
	public:
		// BLAS : construit un BVH par split médian sur les triangles fournis
		// (espace local). RÉORDONNE `triangles` en place pour que chaque
		// feuille référence une plage contiguë [firstTri, firstTri+triCount).
		static std::vector<GPUBVHNode> build(std::vector<GPUTriangle>& triangles, int leafSize = 4);

		// TLAS : construit un BVH sur des boîtes englobantes fournies
		// directement (une par instance, en espace monde). Renvoie en plus la
		// permutation finale dans `order` (order[i] = index d'origine de
		// l'élément placé en position i) pour que l'appelant réordonne son
		// propre tableau parallèle (ex: buffer d'instances) de la même façon.
		static std::vector<GPUBVHNode> buildFromBounds(
			const std::vector<glm::vec3>& boundsMin,
			const std::vector<glm::vec3>& boundsMax,
			std::vector<int>& order,
			int leafSize = 1);
	};
}
