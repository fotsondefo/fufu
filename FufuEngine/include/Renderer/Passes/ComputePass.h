#pragma once

#include "Renderer/GPUBuffers.h"
#include "Renderer/GPUScene.h"
#include <cstdint>

namespace Fufu
{
	// Dispatch du compute shader de path/ray tracing. Consomme une géométrie
	// GPU déjà empaquetée (GPUScene) et une caméra/frame déjà préparées par
	// Renderer (construites à partir de la Scene/ECS, ce qui reste sa
	// responsabilité) ; écrit dans les textures de sortie/accumulation fournies.
	class ComputePass
	{
	public:
		void init();
		void shutdown();

		// skyboxTexture : 0 si aucun skybox actif (frameData.hasSkybox doit
		// alors valoir 0 aussi ; voir Renderer::renderScene).
		void execute(const GPUScene& scene, const GPUCamera& camera, const GPUFrameData& frameData,
			uint32_t outputTexture, uint32_t accumTexture, uint32_t skyboxTexture, int width, int height);

	private:
		uint32_t m_Program = 0;
		uint32_t m_CameraUBO = 0;
		uint32_t m_FrameDataUBO = 0;
	};
}
