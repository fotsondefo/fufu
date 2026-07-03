#pragma once

#include "MeshAsset.h"

namespace Fufu
{
	class MeshUtils
	{
	public:
		// Recalcule les normales par sommet en moyennant les normales
		// géométriques des faces qui le partagent (pondéré par l'aire, puisque
		// non normalisé avant accumulation). Les sommets non partagés entre
		// faces (ex : nos primitives à normales dures type Cube) gardent un
		// rendu "flat" par construction — pas de soudure de topologie ici.
		static void recomputeNormals(SubMesh& mesh);
	};
}
