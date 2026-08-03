#pragma once

#include "Asset.h"

namespace Fufu 
{
	// A shader is defined by two separate source files
	struct ShaderSources
	{
		std::string vertex;
		std::string fragment;
		std::string compute; // Optional — for path tracing
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