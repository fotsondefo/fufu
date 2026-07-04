#pragma once

#include "EnvironmentSettings.h"
#include "Project/Assets/AssetManager.h"
#include <cstdint>
#include <string>

namespace Fufu
{
	// Possède la texture GPU d'un skybox équirectangulaire (RGBA32F si la
	// source est un .hdr, RGBA8 sinon). Ne recharge/reconstruit la texture
	// que quand le chemin change (évite de re-upload une texture potentiellement
	// lourde à chaque frame alors que sceneNeedsUpdate() force déjà un
	// re-upload complet de la géométrie à chaque frame).
	class Skybox
	{
	public:
		void shutdown();

		// À appeler chaque frame : recharge si `settings` a changé depuis le
		// dernier appel, ne fait rien sinon. Si useSkybox est false ou le
		// chemin est vide, libère la texture existante s'il y en avait une.
		void update(const EnvironmentSettings& settings, AssetManager& assetManager);

		bool     isActive() const { return m_TextureID != 0; }
		uint32_t getTextureID() const { return m_TextureID; }

	private:
		uint32_t    m_TextureID = 0;
		std::string m_LoadedPath;
	};
}
