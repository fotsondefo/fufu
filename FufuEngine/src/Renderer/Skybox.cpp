#include "depch.h"
#include "Renderer/Skybox.h"

namespace Fufu
{

	void Skybox::shutdown()
	{
		if (m_TextureID)
		{
			glDeleteTextures(1, &m_TextureID);
			m_TextureID = 0;
		}
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
			return; // déjà à jour

		if (m_TextureID)
		{
			glDeleteTextures(1, &m_TextureID);
			m_TextureID = 0;
		}
		m_LoadedPath.clear();

		auto tex = assetManager.getTexture(settings.skyboxTexturePath);
		if (!tex)
		{
			FUFU_ERROR("Skybox: failed to load texture '{}'", settings.skyboxTexturePath);
			return;
		}

		// Lignes non alignées sur 4 octets (RGB 8 bits, ou toute largeur non
		// multiple de 4) : sans ça, glTexImage2D corrompt l'image lue.
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // longitude : boucle
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // latitude : pas de boucle (pôles)

		m_LoadedPath = settings.skyboxTexturePath;

		FUFU_INFO("Skybox: loaded '{}' ({}x{}{})", m_LoadedPath, tex->getWidth(), tex->getHeight(),
			tex->isHDR() ? ", HDR" : "");
	}

}
