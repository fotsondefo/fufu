#pragma once

#include "Renderer/GPUBuffers.h"
#include "Renderer/GPUScene.h"
#include <cstdint>

namespace Fufu
{
	// Dispatch of the path/ray tracing compute shader. Consumes already-packed
	// GPU geometry (GPUScene) and a camera/frame already prepared by Renderer
	// (built from the Scene/ECS, which remains its responsibility); writes into
	// the provided output/accumulation textures.
	class ComputePass
	{
	public:
		void init();
		void shutdown();

		// skyboxTexture: 0 if no active skybox (frameData.hasSkybox must then
		// also be 0; see Renderer::renderScene).
		void execute(const GPUScene& scene, const GPUCamera& camera, const GPUFrameData& frameData,
			uint32_t outputTexture, uint32_t accumTexture, uint32_t skyboxTexture, int width, int height);

	private:
		uint32_t m_Program = 0;
		uint32_t m_CameraUBO = 0;
		uint32_t m_FrameDataUBO = 0;
	};
}
