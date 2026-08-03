#pragma once

#include <cstdint>

namespace Fufu
{
	// FXAA post-process in a separate pass: reads a source texture (ComputePass
	// output) and writes into ITS OWN texture — it is impossible to read and
	// write the same texture in a single pass. Owns its own FBO/texture,
	// recreated on each resize.
	class FXAAPass
	{
	public:
		void init(int width, int height);
		void shutdown();
		void resize(int width, int height);

		// quadVAO: the shared fullscreen quad, owned by Renderer.
		void execute(uint32_t sourceTexture, uint32_t quadVAO, int width, int height);

		uint32_t getOutputTexture() const { return m_Texture; }

	private:
		void createResources(int width, int height);

		uint32_t m_Program = 0;
		uint32_t m_Texture = 0; // RGBA32F
		uint32_t m_FBO = 0;
	};
}
