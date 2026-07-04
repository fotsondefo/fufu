#pragma once

#include "RenderMode.h"

namespace Fufu
{

	// Orthogonal à RenderMode (qui régit l'affichage/accumulation, pas
	// l'algorithme) : PathTracing fait de la GI diffuse stochastique (bruit
	// qui converge avec le temps), RayTracing est un ray tracer classique
	// (Whitted) — éclairage direct déterministe, réflexions/réfractions
	// récursives, pas de GI diffuse. Pas de bruit, mais pas de lumière
	// indirecte réaliste non plus (juste un léger ambiant plat).
	enum class RenderTechnique { PathTracing, RayTracing };

	// None  : échantillon unique au centre du pixel, pas de lissage — bords
	//         crantés mais résultat déterministe et le moins coûteux.
	// SSAA  : supersampling — jitter sous-pixel différent à chaque `sample`
	//         de `samplesPerPixel`, moyennés dans la même frame (et encore
	//         accumulés frame à frame en mode Accumulation). Le comportement
	//         historique du renderer avant l'ajout de ce réglage.
	// TAA   : un seul sample par frame (jitter sous-pixel qui varie à chaque
	//         frame), lissé par une moyenne mobile exponentielle avec
	//         l'historique — indépendant du RenderMode, donc lisse même en
	//         Realtime où SSAA/None ne peuvent pas accumuler.
	// FXAA  : pas de jitter (échantillon net), lissage en post-process par
	//         détection de contraste de luminance sur l'image finale — le
	//         moins cher, ne demande aucun sample supplémentaire.
	enum class AAMode { None, SSAA, TAA, FXAA };

	struct RenderSettings
	{
		RenderMode      mode = RenderMode::Accumulation;
		RenderTechnique technique = RenderTechnique::PathTracing;
		AAMode     aaMode = AAMode::SSAA;
		int        maxBounces = 8;      // Profondeur max de rebonds
		int        samplesPerPixel = 1;      // Samples par pixel par frame
		int        maxAccumFrames = 2048;   // Limite accumulation avant arr�t
		float      exposure = 1.f;
		float      taaBlendFactor = 0.9f; // Poids de l'historique en mode TAA (0 = pas d'historique, proche de 1 = très lisse mais plus de rémanence)
		bool       resetOnMove = true;   // Remet l'accumulation � z�ro si cam�ra bouge
	};

}