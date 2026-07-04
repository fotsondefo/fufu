#pragma once

#include <cstdint>

namespace Fufu
{
	// Post-process FXAA en pass séparée : lit une texture source (sortie du
	// ComputePass) et écrit dans SA PROPRE texture — impossible de lire et
	// écrire la même texture dans un seul pass. Possède son propre
	// FBO/texture, recréés à chaque resize.
	class FXAAPass
	{
	public:
		void init(int width, int height);
		void shutdown();
		void resize(int width, int height);

		// quadVAO : le quad plein écran partagé, possédé par Renderer.
		void execute(uint32_t sourceTexture, uint32_t quadVAO, int width, int height);

		uint32_t getOutputTexture() const { return m_Texture; }

	private:
		void createResources(int width, int height);

		uint32_t m_Program = 0;
		uint32_t m_Texture = 0; // RGBA32F
		uint32_t m_FBO = 0;
	};
}
