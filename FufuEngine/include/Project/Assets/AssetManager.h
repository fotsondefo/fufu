#pragma once

#include "Asset.h"
#include "TextureAsset.h"
#include "MeshAsset.h"
#include "ShaderAsset.h"

namespace Fufu 
{

	class AssetManager
	{
	public:
		// Singleton
		static AssetManager& get()
		{
			static AssetManager instance;
			return instance;
		}

		// Enregistre un asset sans le charger (crée le .meta si absent)
		UUID registerAsset(const std::filesystem::path& path, AssetType type);

		// Accès typé avec chargement différé
		template<typename T>
		std::shared_ptr<T> getAsset(UUID uuid)
		{
			auto it = m_Pool.find(uuid);
			if (it == m_Pool.end())
			{
				FUFU_ERROR("AssetManager: unknown UUID {}", uuid.value());
				return nullptr;
			}

			auto& asset = it->second;

			if (asset->getMeta().state == AssetState::Unloaded)
				loadAsset(asset);

			if (asset->getMeta().state != AssetState::Loaded)
				return nullptr;

			return std::dynamic_pointer_cast<T>(asset);
		}

		// Raccourcis directs par chemin
		std::shared_ptr<TextureAsset> getTexture(const std::filesystem::path& path);
		std::shared_ptr<MeshAsset>    getMesh(const std::filesystem::path& path);
		std::shared_ptr<ShaderAsset>  getShader(const std::filesystem::path& vertPath,
			const std::filesystem::path& fragPath,
			const std::filesystem::path& computePath = "");

		// Décharge un asset de la mémoire (repassera Unloaded)
		void unload(UUID uuid);
		void unloadAll();

		// Lit/écrit le fichier .meta associé à un chemin
		void        writeMeta(const AssetMeta& meta) const;
		std::optional<AssetMeta> readMeta(const std::filesystem::path& path) const;

		bool        hasAsset(UUID uuid) const { return m_Pool.count(uuid) > 0; }
		size_t      assetCount()        const { return m_Pool.size(); }

	private:
		AssetManager() = default;

		void loadAsset(std::shared_ptr<Asset>& asset);
		void loadTexture(std::shared_ptr<TextureAsset>& asset);
		void loadMesh(std::shared_ptr<MeshAsset>&    asset);
		void loadShader(std::shared_ptr<ShaderAsset>&  asset);

		std::filesystem::path metaPath(const std::filesystem::path& sourcePath) const;

		// Pool principal UUID ? Asset
		std::unordered_map<UUID, std::shared_ptr<Asset>> m_Pool;

		// Index chemin ? UUID pour les lookups directs
		std::unordered_map<std::string, UUID> m_PathIndex;
	};

}