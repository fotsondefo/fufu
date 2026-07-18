#pragma once

#include "Renderer/GPUScene.h"
#include <cstdint>
#include <glm/glm.hpp>

namespace Fufu
{
	// Passe de rasterisation G-Buffer (MRT) : position monde, normale monde,
	// UV + index de matériau. Utilisée par la technique Deferred Rendering.
	// Les SSBOs (positions, attributs) doivent être bindés avant render().
	class GBufferPass
	{
	public:
		void init(int width, int height);
		void shutdown();
		void resize(int width, int height);

		// Rasterise toutes les instances de gpu dans les trois textures MRT.
		// gpu.bind() doit avoir été appelé par l'appelant (ou cette méthode
		// l'appelle elle-même — voir impl).
		void render(const GPUScene& gpu, const glm::mat4& viewProj, int width, int height);

		uint32_t getPositionTexture() const { return m_PositionTex; }
		uint32_t getNormalTexture()   const { return m_NormalTex; }
		uint32_t getUVTexture()       const { return m_UVTex; }

	private:
		void createFBO(int w, int h);
		void deleteFBO();

		uint32_t m_Program    = 0;
		uint32_t m_DummyVAO   = 0;
		uint32_t m_FBO        = 0;
		uint32_t m_PositionTex = 0;
		uint32_t m_NormalTex   = 0;
		uint32_t m_UVTex       = 0;
		uint32_t m_DepthRBO    = 0;

		int m_LocViewProj     = -1;
		int m_LocTransform    = -1;
		int m_LocInvTransform = -1;
		int m_LocTriOffset    = -1;
		int m_LocMatIdx       = -1;
	};
}
