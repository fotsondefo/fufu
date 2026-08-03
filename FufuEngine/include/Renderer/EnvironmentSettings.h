#pragma once

#include <string>

namespace Fufu
{
	// Scene environment: what a ray "sees" when it does not hit any geometry.
	// useSkybox=false keeps the existing procedural sky gradient (see sampleSky()
	// in the compute shader); otherwise an equirectangular HDRI texture (.hdr)
	// is sampled instead, also used for background environment lighting (no
	// irradiance convolution: it is a direct sample, no GI from the sky).
	struct EnvironmentSettings
	{
		bool        useSkybox = false;
		std::string skyboxTexturePath;
		float       skyboxIntensity = 1.f;
	};
}
