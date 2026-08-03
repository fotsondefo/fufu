#pragma once

#include "UUID.h"

namespace Fufu 
{

	enum class AssetType
	{
		None = 0,
		Texture,
		Mesh,
		Shader
	};

	enum class AssetState
	{
		Unloaded,   // Registered but not yet loaded
		Loading,    // In progress (reserved for async later)
		Loaded,     // Ready to use
		Failed      // Error during loading
	};

	struct AssetMeta
	{
		UUID  uuid;
		std::filesystem::path sourcePath;
		AssetType type = AssetType::None;
		AssetState state = AssetState::Unloaded;
	};

	// Base class for all assets
	class Asset
	{
	public:
		virtual ~Asset() = default;
		virtual AssetType getType() const = 0;

		const AssetMeta& getMeta() const { return m_Meta; }
		bool isLoaded() const { return m_Meta.state == AssetState::Loaded; }

	protected:
		AssetMeta m_Meta;
		friend class AssetManager;
	};

}