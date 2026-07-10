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
		explicit AssetManager(const std::filesystem::path& rootDir);
		~AssetManager() = default;

		// Scan the root directory when the project loads
		void scanDirectory();

		UUID registerAsset(const std::filesystem::path& path, AssetType type);

		// Lazy loading to asset
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

		// Shortcuts
		std::shared_ptr<TextureAsset> getTexture(const std::filesystem::path& path);
		std::shared_ptr<MeshAsset> getMesh(const std::filesystem::path& path);
		std::shared_ptr<ShaderAsset>  getShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath, const std::filesystem::path& computePath = "");

		void unload(UUID uuid);
		void unloadAll();

		bool   hasAsset(UUID uuid)  const { return m_Pool.count(uuid) > 0; }
		size_t assetCount() const { return m_Pool.size(); }

		// Permet à un appelant de distinguer "pas encore prêt, chargement en
		// arrière-plan en cours" (Loading, retentera de lui-même) d'un
		// véritable échec (Failed, inutile de réessayer) — les deux se
		// traduisent par un getAsset<T>() qui renvoie nullptr.
		AssetState getAssetState(UUID uuid) const
		{
			auto it = m_Pool.find(uuid);
			return it != m_Pool.end() ? it->second->getMeta().state : AssetState::Unloaded;
		}

		const std::filesystem::path& getRootDir() const { return m_RootDir; }

		const std::unordered_map<UUID, std::shared_ptr<Asset>>& getPool() const
		{
			return m_Pool;
		}

		void writeMeta(const AssetMeta& meta) const;
		std::optional<AssetMeta> readMeta(const std::filesystem::path& path) const;

	private:
		// Déclenche le chargement en arrière-plan (voir JobSystem) : bascule
		// l'état sur Loading immédiatement (synchrone), puis soumet un job qui
		// remplit les données CPU de l'asset et ne bascule l'état sur
		// Loaded/Failed QUE via son callback main-thread — jamais depuis le
		// thread de fond, pour ne jamais lire/écrire m_Meta.state depuis deux
		// threads à la fois.
		void loadAsset(std::shared_ptr<Asset>& asset);

		// Remplissent les données CPU de l'asset et renvoient true en cas de
		// succès — NE touchent PAS m_Meta.state (voir loadAsset). Appelées
		// depuis un thread de fond : ne doivent jamais toucher OpenGL.
		bool loadTexture(std::shared_ptr<TextureAsset>& asset);
		bool loadMesh(std::shared_ptr<MeshAsset>&    asset);
		bool loadShader(std::shared_ptr<ShaderAsset>&  asset);

		AssetType inferTypeFromExtension(const std::filesystem::path& path) const;
		std::filesystem::path metaPath(const std::filesystem::path& sourcePath) const;

		std::filesystem::path m_RootDir;
		std::unordered_map<UUID, std::shared_ptr<Asset>> m_Pool;
		std::unordered_map<std::string, UUID> m_PathIndex;
	};

}