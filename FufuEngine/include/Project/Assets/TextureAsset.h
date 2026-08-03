#pragma once

#include "Asset.h"

namespace Fufu 
{

	struct TextureData
	{
		int            width = 0;
		int            height = 0;
		int            channels = 0;
		bool           isHDR = false;        // .hdr: floatPixels filled instead of pixels
		unsigned char* pixels = nullptr;     // LDR: raw stb_image data (8 bits)
		float*         floatPixels = nullptr; // HDR: raw stb_image data (32-bit float)
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

		// Pointer to raw pixels (valid as long as the asset is Loaded).
		// Use getPixels() if !isHDR(), getFloatPixels() if isHDR().
		const unsigned char* getPixels()      const { return m_Data.pixels; }
		const float*         getFloatPixels() const { return m_Data.floatPixels; }

	private:
		TextureData m_Data;
		friend class AssetManager;
	};

}