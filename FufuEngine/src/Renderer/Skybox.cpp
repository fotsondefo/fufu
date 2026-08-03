#include "depch.h"
#include "Renderer/Skybox.h"

namespace Fufu
{

	void Skybox::init()
	{
		m_Baker.init();
	}

	void Skybox::shutdown()
	{
		if (m_TextureID)    { glDeleteTextures(1, &m_TextureID);    m_TextureID    = 0; }
		if (m_IrradianceMap)  { glDeleteTextures(1, &m_IrradianceMap);  m_IrradianceMap  = 0; }
		if (m_PrefilteredMap) { glDeleteTextures(1, &m_PrefilteredMap); m_PrefilteredMap = 0; }
		m_Baker.shutdown();
		m_LoadedPath.clear();
	}

	void Skybox::update(const EnvironmentSettings& settings, AssetManager& assetManager)
	{
		if (!settings.useSkybox || settings.skyboxTexturePath.empty())
		{
			if (m_TextureID) shutdown();
			return;
		}

		if (m_TextureID != 0 && settings.skyboxTexturePath == m_LoadedPath)
			return; // already up to date

		if (m_TextureID)
		{
			glDeleteTextures(1, &m_TextureID);
			m_TextureID = 0;
		}
		m_LoadedPath.clear();

		auto tex = assetManager.getTexture(settings.skyboxTexturePath);
		if (!tex)
		{
			// getTexture() returns nullptr both while a background load is
			// still in progress (see JobSystem/AssetManager) and on actual
			// failure — this function runs every frame, so it retries
			// automatically while loading, without spamming the log.
			// We log only once if the load has truly failed.
			UUID uuid = assetManager.registerAsset(settings.skyboxTexturePath, AssetType::Texture);
			if (assetManager.getAssetState(uuid) == AssetState::Failed &&
				m_LastLoggedFailurePath != settings.skyboxTexturePath)
			{
				FUFU_ERROR("Skybox: failed to load texture '{}'", settings.skyboxTexturePath);
				m_LastLoggedFailurePath = settings.skyboxTexturePath;
			}
			return;
		}

		// Rows not aligned to 4 bytes (8-bit RGB, or any width not a
		// multiple of 4): without this, glTexImage2D corrupts the image.
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glGenTextures(1, &m_TextureID);
		glBindTexture(GL_TEXTURE_2D, m_TextureID);

		if (tex->isHDR())
		{
			GLenum format = (tex->getChannels() >= 4) ? GL_RGBA : GL_RGB;
			GLenum internalFormat = (tex->getChannels() >= 4) ? GL_RGBA32F : GL_RGB32F;
			glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, tex->getWidth(), tex->getHeight(), 0,
				format, GL_FLOAT, tex->getFloatPixels());
		}
		else
		{
			GLenum format = (tex->getChannels() >= 4) ? GL_RGBA : (tex->getChannels() == 1 ? GL_RED : GL_RGB);
			GLenum internalFormat = (tex->getChannels() >= 4) ? GL_RGBA8 : (tex->getChannels() == 1 ? GL_R8 : GL_RGB8);
			glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, tex->getWidth(), tex->getHeight(), 0,
				format, GL_UNSIGNED_BYTE, tex->getPixels());
		}

		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // longitude: wraps
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // latitude: no wrap (poles)

		m_LoadedPath = settings.skyboxTexturePath;

		FUFU_INFO("Skybox: loaded '{}' ({}x{}{})", m_LoadedPath, tex->getWidth(), tex->getHeight(),
			tex->isHDR() ? ", HDR" : "");

		// Bake IBL textures from the newly loaded environment map.
		m_Baker.bake(m_TextureID, m_IrradianceMap, m_PrefilteredMap);
	}

}
