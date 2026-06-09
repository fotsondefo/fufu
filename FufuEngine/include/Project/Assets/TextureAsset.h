#pragma once

#include "Asset.h"

namespace Fufu 
{

	struct TextureData
	{
		int            width = 0;
		int            height = 0;
		int            channels = 0;
		unsigned char* pixels = nullptr; // Données brutes stb_image
	};

	class TextureAsset : public Asset
	{
	public:
		~TextureAsset() override;

		AssetType getType() const override { return AssetType::Texture; }

		int getWidth()    const { return m_Data.width; }
		int getHeight()   const { return m_Data.height; }
		int getChannels() const { return m_Data.channels; }

		// Pointeur vers les pixels bruts (valide tant que l'asset est Loaded)
		const unsigned char* getPixels() const { return m_Data.pixels; }

	private:
		TextureData m_Data;
		friend class AssetManager;
	};

}