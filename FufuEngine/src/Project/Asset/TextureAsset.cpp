#include "depch.h"
#include "Project/Assets/TextureAsset.h"
#include <stb_image.h>

namespace Fufu 
{

	TextureAsset::~TextureAsset()
	{
		if (m_Data.pixels)
			stbi_image_free(m_Data.pixels);
	}

}