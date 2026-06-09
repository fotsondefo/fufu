#pragma once

#include "Asset.h"

namespace Fufu 
{
	// Un shader est défini par deux fichiers source séparés
	struct ShaderSources
	{
		std::string vertex;
		std::string fragment;
		std::string compute; // Optionnel — pour le path tracing
	};

	class ShaderAsset : public Asset
	{
	public:
		AssetType getType() const override { return AssetType::Shader; }

		const ShaderSources& getSources() const { return m_Sources; }

	private:
		ShaderSources m_Sources;
		friend class AssetManager;
	};

}