#pragma once

#include "Renderer/GPUScene.h"
#include <cstdint>
#include <glm/glm.hpp>

namespace Fufu
{
	// Passe d'éclairage différé : lit le G-Buffer (position/normale/UV)
	// et applique PBR Cook-Torrance en fullscreen. Utilise FullscreenQuad.vert
	// + DeferredLighting.frag. Les pixels sans géométrie reçoivent le ciel.
	class DeferredPass
	{
	public:
		void init(int width, int height);
		void shutdown();
		void resize(int width, int height);

		void render(const GPUScene& gpu,
		            uint32_t gPosition,
		            uint32_t gNormal,
		            uint32_t gUV,
		            uint32_t quadVAO,
		            uint32_t skyboxTex,
		            bool hasSkybox,
		            float skyboxIntensity,
		            const glm::vec3& camPos,
		            const glm::vec3& camForward,
		            const glm::vec3& camRight,
		            const glm::vec3& camUp,
		            float camFov,
		            float camAspect,
		            float exposure,
		            int width, int height);

		uint32_t getOutputTexture() const { return m_OutputTex; }

	private:
		void createFBO(int w, int h);
		void deleteFBO();

		uint32_t m_Program   = 0;
		uint32_t m_FBO       = 0;
		uint32_t m_OutputTex = 0;

		int m_LocCamPos          = -1;
		int m_LocCamForward      = -1;
		int m_LocCamRight        = -1;
		int m_LocCamUp           = -1;
		int m_LocCamFov          = -1;
		int m_LocCamAspect       = -1;
		int m_LocExposure        = -1;
		int m_LocLightCount      = -1;
		int m_LocHasSkybox       = -1;
		int m_LocSkyboxIntensity = -1;
	};
}
