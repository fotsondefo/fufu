#pragma once

#include "Renderer/GPUScene.h"
#include "Renderer/RasterUniforms.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace Fufu
{
	// Directional shadow map rendering: depth-only 2048x2048,
	// vertex-pulling from PositionBuffer (SSBO binding=2),
	// UBO binding=0 = ShadowBlock (lightSpaceMatrix),
	// UBO binding=1 = DrawBlock (transform + triOffset, one draw per instance).
	// The resulting texture (sampler2DShadow) is consumed by DeferredLighting.
	class ShadowPass
	{
	public:
		static constexpr int kSize = 2048;

		void init();
		void shutdown();

		void execute(const GPUScene& gpu, const glm::mat4& lightSpaceMatrix);

		uint32_t getDepthTexture() const { return m_DepthTexture; }

	private:
		uint32_t m_Program      = 0;
		uint32_t m_VAO          = 0; // dummy VAO required by OpenGL
		uint32_t m_FBO          = 0;
		uint32_t m_DepthTexture = 0;
		uint32_t m_ShadowUBO    = 0; // binding 0: mat4 lightSpaceMatrix
		uint32_t m_DrawUBO      = 0; // binding 1: GPUDrawUBO
	};
}
