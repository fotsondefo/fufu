#pragma once

#include "EnvironmentSettings.h"
#include "IBLBaker.h"
#include "Project/Assets/AssetManager.h"
#include <cstdint>
#include <string>

namespace Fufu
{
	// Owns the GPU texture of an equirectangular skybox (RGBA32F if the source
	// is a .hdr, RGBA8 otherwise) and the three IBL textures derived from it
	// (irradiance map, pre-filtered env map, BRDF LUT).
	// Reloads/rebuilds when the path changes; re-bakes IBL whenever the env
	// map is replaced.

	class Skybox
	{
	public:
		// Must be called once after the GL context is ready (compiles IBL shaders
		// and pre-computes the BRDF LUT).
		void init();
		void shutdown();

		// To be called every frame: reloads if `settings` has changed since the
		// last call, does nothing otherwise. If useSkybox is false or the path
		// is empty, releases the existing texture if there was one.
		void update(const EnvironmentSettings& settings, AssetManager& assetManager);

		bool     isActive()         const { return m_TextureID != 0; }
		uint32_t getTextureID()     const { return m_TextureID; }

		// IBL textures — valid only when isActive() is true.
		uint32_t getIrradianceMap()  const { return m_IrradianceMap; }
		uint32_t getPrefilteredMap() const { return m_PrefilteredMap; }
		uint32_t getBrdfLut()        const { return m_Baker.getBrdfLut(); }

	private:
		uint32_t    m_TextureID = 0;
		uint32_t    m_IrradianceMap  = 0;
		uint32_t    m_PrefilteredMap = 0;
		std::string m_LoadedPath;

		// Avoids spamming the log every frame as long as a load failure (not
		// just "still in progress") has not already been reported for this
		// path — see update().
		std::string m_LastLoggedFailurePath;

		IBLBaker m_Baker;
	};
}
