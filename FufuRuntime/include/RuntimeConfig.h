#pragma once

namespace FufuRuntime
{
	struct RuntimeConfig
	{
		std::string scenePath = "default.fufuscene";
		std::string windowTitle = "Fufu Runtime";
		int width = 1280;
		int height = 720;
		bool vsync = true;

		// Render settings
		int maxBounces = 8;
		int samplesPerPixel = 1;
		float exposure = 1.f;
	};
}