#pragma once

#include "MeshAsset.h"

namespace Fufu
{
	class MeshSimplifier
	{
	public:
		// Simplification par clustering de sommets sur une grille de cellules
		// de taille `cellSize` (mêmes unités que les positions du mesh) :
		// les sommets tombant dans la même cellule fusionnent en un seul
		// (moyenne position/normale/uv), les triangles devenus dégénérés sont
		// supprimés. Volontairement simple (pas de quadric error metric) —
		// rapide, topologie non garantie optimale, mais un LOD "assez bon"
		// pour un faible coût d'implémentation. Voir MeshAsset::getLODSubMeshes.
		static SubMesh simplify(const SubMesh& source, float cellSize);
	};
}
