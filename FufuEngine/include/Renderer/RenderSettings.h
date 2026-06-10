#pragma once

#include "RenderMode.h"

namespace Fufu 
{

	struct RenderSettings
	{
		RenderMode mode = RenderMode::Accumulation;
		int        maxBounces = 8;      // Profondeur max de rebonds
		int        samplesPerPixel = 1;      // Samples par pixel par frame
		int        maxAccumFrames = 2048;   // Limite accumulation avant arrêt
		float      exposure = 1.f;
		bool       resetOnMove = true;   // Remet l'accumulation à zéro si caméra bouge
	};

}