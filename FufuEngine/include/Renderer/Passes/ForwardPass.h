#pragma once

#include "Renderer/GPUScene.h"
#include <cstdint>
#include <glm/glm.hpp>

namespace Fufu
{
	// Rendu forward PBR : ciel en background, puis géométrie avec éclairage
	// Cook-Torrance par fragment. Vertex fetch depuis les SSBOs (pas de VAO).
	class ForwardPass
	{
	public:
		void init(int width, int height);
		void shutdown();
		void resize(int width, int height);

		void render(const GPUScene& gpu,
		            const glm::mat4& viewProj,
		            const glm::vec3& camPos,
		            const glm::vec3& camForward,
		            const glm::vec3& camRight,
		            const glm::vec3& camUp,
		            float camFov,           // radians
		            float camAspect,
		            uint32_t quadVAO,
		            uint32_t skyboxTex,
		            bool hasSkybox,
		            float skyboxIntensity,
		            float exposure,
		            int width, int height);

		uint32_t getOutputTexture() const { return m_OutputTex; }

	private:
		void createFBO(int w, int h);
		void deleteFBO();

		uint32_t m_SkyProgram = 0; // FullscreenQuad.vert + Sky.frag
		uint32_t m_GeoProgram = 0; // GBuffer.vert + Forward.frag
		uint32_t m_DummyVAO   = 0;
		uint32_t m_FBO        = 0;
		uint32_t m_OutputTex  = 0;
		uint32_t m_DepthRBO   = 0;

		// Sky uniforms
		int m_SkyLocCamForward       = -1;
		int m_SkyLocCamRight         = -1;
		int m_SkyLocCamUp            = -1;
		int m_SkyLocCamFov           = -1;
		int m_SkyLocCamAspect        = -1;
		int m_SkyLocHasSkybox        = -1;
		int m_SkyLocSkyboxIntensity  = -1;
		int m_SkyLocExposure         = -1;

		// Geometry uniforms
		int m_GeoLocViewProj     = -1;
		int m_GeoLocTransform    = -1;
		int m_GeoLocInvTransform = -1;
		int m_GeoLocTriOffset    = -1;
		int m_GeoLocMatIdx       = -1;
		int m_GeoLocCamPos       = -1;
		int m_GeoLocExposure     = -1;
		int m_GeoLocLightCount   = -1;
	};
}
