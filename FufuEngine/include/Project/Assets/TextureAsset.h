#pragma once

#include "Asset.h"

namespace Fufu 
{

	struct TextureData
	{
		int            width = 0;
		int            height = 0;
		int            channels = 0;
		bool           isHDR = false;        // .hdr : floatPixels rempli plutôt que pixels
		unsigned char* pixels = nullptr;     // LDR : donn�es brutes stb_image (8 bits)
		float*         floatPixels = nullptr; // HDR : donn�es brutes stb_image (32 bits flottant)
	};

	class TextureAsset : public Asset
	{
	public:
		~TextureAsset() override;

		AssetType getType() const override { return AssetType::Texture; }

		int getWidth()    const { return m_Data.width; }
		int getHeight()   const { return m_Data.height; }
		int getChannels() const { return m_Data.channels; }
		bool isHDR()      const { return m_Data.isHDR; }

		// Pointeur vers les pixels bruts (valide tant que l'asset est Loaded).
		// Utiliser getPixels() si !isHDR(), getFloatPixels() si isHDR().
		const unsigned char* getPixels()      const { return m_Data.pixels; }
		const float*         getFloatPixels() const { return m_Data.floatPixels; }

	private:
		TextureData m_Data;
		friend class AssetManager;
	};

}