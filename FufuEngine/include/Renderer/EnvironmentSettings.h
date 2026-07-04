#pragma once

#include <string>

namespace Fufu
{
	// Environnement de la scène : ce qu'un rayon "voit" quand il ne touche
	// aucune géométrie. useSkybox=false garde le dégradé de ciel procédural
	// existant (voir sampleSky() dans le compute shader) ; sinon une texture
	// HDRI équirectangulaire (.hdr) est échantillonnée à la place, aussi
	// utilisée pour l'éclairage d'environnement du fond (pas de convolution
	// d'irradiance : c'est un échantillon direct, pas de GI depuis le ciel).
	struct EnvironmentSettings
	{
		bool        useSkybox = false;
		std::string skyboxTexturePath;
		float       skyboxIntensity = 1.f;
	};
}
