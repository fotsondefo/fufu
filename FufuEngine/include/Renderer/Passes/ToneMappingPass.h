#pragma once

#include "Renderer/RenderSettings.h"
#include <cstdint>

namespace Fufu
{
	// Fullscreen post-process: converts the linear HDR texture to LDR sRGB
	// via the selected operator (None/Reinhard/ACES/Filmic) + gamma correction.
	// Inserted after each raster/compute pass, before the optional FXAA.
	class ToneMappingPass
	{
	public:
		void init(int width, int height);
		void shutdown();
		void resize(int width, int height);

		void execute(uint32_t sourceTexture,
		             uint32_t quadVAO,
		             int width, int height,
		             ToneMappingOperator op,
		             float gamma);

		uint32_t getOutputTexture() const { return m_Texture; }

	private:
		void createResources(int width, int height);

		uint32_t m_Program = 0;
		uint32_t m_Texture = 0;
		uint32_t m_FBO     = 0;
	};
}
