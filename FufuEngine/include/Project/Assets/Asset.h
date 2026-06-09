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
		Unloaded,   // Enregistré mais pas encore chargé
		Loading,    // En cours (réservé pour async plus tard)
		Loaded,     // Prêt à l'emploi
		Failed      // Erreur au chargement
	};

	struct AssetMeta
	{
		UUID  uuid;
		std::filesystem::path sourcePath;
		AssetType type = AssetType::None;
		AssetState state = AssetState::Unloaded;
	};

	// Classe de base pour tous les assets
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