#include "depch.h"
#include "Project/Assets/AssetManager.h"
#include <nlohmann/json.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <fstream>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

using json = nlohmann::json;

namespace Fufu 
{

	AssetManager::AssetManager(const std::filesystem::path& rootDir)
		: m_RootDir(rootDir)
	{
		FUFU_INFO("AssetManager created � root: '{}'", rootDir.string());
	}

	void AssetManager::scanDirectory()
	{
		if (!std::filesystem::exists(m_RootDir))
		{
			FUFU_WARN("AssetManager: root directory does not exist: '{}'",
				m_RootDir.string());
			return;
		}

		size_t count = 0;
		for (const auto& entry :
			std::filesystem::recursive_directory_iterator(m_RootDir))
		{
			if (!entry.is_regular_file()) continue;

			const auto& path = entry.path();

			// Ignore the meta files
			if (path.extension() == ".meta") continue;

			AssetType type = inferTypeFromExtension(path);
			if (type == AssetType::None) continue;

			registerAsset(path, type);
			++count;
		}

		FUFU_INFO("AssetManager: scanned {} assets in '{}'", count, m_RootDir.string());
	}

	AssetType AssetManager::inferTypeFromExtension(const std::filesystem::path& path) const
	{
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
			return AssetType::Texture;

		if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae")
			return AssetType::Mesh;

		if (ext == ".vert" || ext == ".frag" || ext == ".comp" || ext == ".glsl")
			return AssetType::Shader;

		return AssetType::None;
	}

	std::filesystem::path AssetManager::metaPath(const std::filesystem::path& sourcePath) const
	{
		return sourcePath.string() + ".meta";
	}

	void AssetManager::writeMeta(const AssetMeta& meta) const
	{
		json j;
		j["uuid"] = meta.uuid.value();
		j["path"] = meta.sourcePath.string();
		j["type"] = static_cast<int>(meta.type);

		std::ofstream file(metaPath(meta.sourcePath));
		if (file.is_open())
			file << j.dump(4);
	}

	std::optional<AssetMeta> AssetManager::readMeta(const std::filesystem::path& path) const
	{
		std::ifstream file(metaPath(path));
		if (!file.is_open())
			return std::nullopt;

		json j;
		try { 
			j = json::parse(file); 
		}
		catch (...) { 
			return std::nullopt; 
		}

		AssetMeta meta;
		meta.uuid = UUID(j.at("uuid").get<uint64_t>());
		meta.sourcePath = j.at("path").get<std::string>();
		meta.type = static_cast<AssetType>(j.at("type").get<int>());
		meta.state = AssetState::Unloaded;
		
		return meta;
	}

	UUID AssetManager::registerAsset(const std::filesystem::path& path, AssetType type)
	{
		std::string canonical = std::filesystem::weakly_canonical(path).string();

		auto it = m_PathIndex.find(canonical);
		if (it != m_PathIndex.end())
			return it->second;

		// if the meta file exist we read it. Otherwise we create one.
		AssetMeta meta;
		auto existing = readMeta(path);
		if (existing.has_value())
		{
			meta = existing.value();
		}
		else
		{
			meta.uuid = UUID::fromPath(canonical);
			meta.sourcePath = path;
			meta.type = type;
			meta.state = AssetState::Unloaded;
			writeMeta(meta);
		}

		// Asset creation
		std::shared_ptr<Asset> asset;
		switch (type)
		{
		case AssetType::Texture: asset = std::make_shared<TextureAsset>(); break;
		case AssetType::Mesh:    asset = std::make_shared<MeshAsset>();    break;
		case AssetType::Shader:  asset = std::make_shared<ShaderAsset>();  break;
		default:
			FUFU_ERROR("AssetManager: unknown asset type for '{}'", canonical);
			return UUID{};
		}

		asset->m_Meta = meta;
		m_Pool[meta.uuid] = asset;
		m_PathIndex[canonical] = meta.uuid;

		FUFU_TRACE("Asset registered: '{}' ? UUID {}", canonical, meta.uuid.value());

		return meta.uuid;
	}

	void AssetManager::loadAsset(std::shared_ptr<Asset>& asset)
	{
		asset->m_Meta.state = AssetState::Loading;

		switch (asset->getType())
		{
		case AssetType::Texture:
		{
			auto tex = std::dynamic_pointer_cast<TextureAsset>(asset);
			loadTexture(tex);
			break;
		}
		case AssetType::Mesh:
		{
			auto mesh = std::dynamic_pointer_cast<MeshAsset>(asset);
			loadMesh(mesh);
			break;
		}
		case AssetType::Shader:
		{
			auto shader = std::dynamic_pointer_cast<ShaderAsset>(asset);
			loadShader(shader);
			break;
		}
		default: break;
		}
	}

	void AssetManager::loadTexture(std::shared_ptr<TextureAsset>& asset)
	{
		const std::string path = asset->m_Meta.sourcePath.string();

		std::string ext = asset->m_Meta.sourcePath.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		asset->m_Data.isHDR = (ext == ".hdr");

		// Flip vertical : utile pour les textures de mesh (convention UV
		// OpenGL, v=0 en bas), mais casserait une HDRI équirectangulaire —
		// sampleSky() suppose que la première ligne de l'image est le zénith
		// (v=0 -> dir.y=1), pas le contraire.
		stbi_set_flip_vertically_on_load(!asset->m_Data.isHDR);

		if (asset->m_Data.isHDR)
		{
			// HDRI équirectangulaire (skybox) : données flottantes, pas de
			// clamp 0-1 comme le 8 bits classique.
			asset->m_Data.floatPixels = stbi_loadf(
				path.c_str(),
				&asset->m_Data.width,
				&asset->m_Data.height,
				&asset->m_Data.channels,
				0
			);

			if (!asset->m_Data.floatPixels)
			{
				FUFU_ERROR("TextureAsset: failed to load HDR '{}'", path);
				asset->m_Meta.state = AssetState::Failed;
				return;
			}
		}
		else
		{
			asset->m_Data.pixels = stbi_load(
				path.c_str(),
				&asset->m_Data.width,
				&asset->m_Data.height,
				&asset->m_Data.channels,
				0
			);

			if (!asset->m_Data.pixels)
			{
				FUFU_ERROR("TextureAsset: failed to load '{}'", path);
				asset->m_Meta.state = AssetState::Failed;
				return;
			}
		}

		asset->m_Meta.state = AssetState::Loaded;
		FUFU_INFO("Texture loaded: '{}' ({}x{} ch:{}{})",
			path, asset->m_Data.width, asset->m_Data.height, asset->m_Data.channels,
			asset->m_Data.isHDR ? ", HDR" : "");
	}

	void AssetManager::loadMesh(std::shared_ptr<MeshAsset>& asset)
	{
		const std::string path = asset->m_Meta.sourcePath.string();

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_Triangulate |
			aiProcess_GenNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_FlipUVs
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			FUFU_ERROR("MeshAsset: Assimp error for '{}': {}", path, importer.GetErrorString());
			asset->m_Meta.state = AssetState::Failed;
			return;
		}

		for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
		{
			aiMesh* aiMesh = scene->mMeshes[m];
			SubMesh sub;
			sub.name = aiMesh->mName.C_Str();

			sub.vertices.reserve(aiMesh->mNumVertices);
			for (unsigned int v = 0; v < aiMesh->mNumVertices; ++v)
			{
				Vertex vertex;
				vertex.position = { aiMesh->mVertices[v].x, aiMesh->mVertices[v].y, aiMesh->mVertices[v].z };
				vertex.normal = { aiMesh->mNormals[v].x, aiMesh->mNormals[v].y, aiMesh->mNormals[v].z };
				vertex.uv = aiMesh->mTextureCoords[0] ? glm::vec2(aiMesh->mTextureCoords[0][v].x, aiMesh->mTextureCoords[0][v].y) : glm::vec2(0.f);
				vertex.tangent = aiMesh->mTangents ? glm::vec3(aiMesh->mTangents[v].x, aiMesh->mTangents[v].y, aiMesh->mTangents[v].z) : glm::vec3(0.f);

				sub.vertices.push_back(vertex);
			}

			sub.indices.reserve(aiMesh->mNumFaces * 3);

			for (unsigned int f = 0; f < aiMesh->mNumFaces; ++f)
				for (unsigned int i = 0; i < aiMesh->mFaces[f].mNumIndices; ++i)
					sub.indices.push_back(aiMesh->mFaces[f].mIndices[i]);

			asset->m_SubMeshes.push_back(std::move(sub));
		}

		asset->m_Meta.state = AssetState::Loaded;
		FUFU_INFO("Mesh loaded: '{}' ({} submeshes)", path, asset->m_SubMeshes.size());
	}

	void AssetManager::loadShader(std::shared_ptr<ShaderAsset>& asset)
	{
		// sourcePath contains the path to the vertex shader
		// The fragment shader is inferred from the .meta file 
		auto readFile = [](const std::filesystem::path& p) -> std::string
		{
			std::ifstream file(p);
			if (!file.is_open())
				return {};
			return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
		};

		// Read the .meta to get the fragment and compute path
		auto metaOpt = readMeta(asset->m_Meta.sourcePath);
		if (!metaOpt.has_value())
		{
			FUFU_ERROR("ShaderAsset: no .meta for '{}'", asset->m_Meta.sourcePath.string());
			asset->m_Meta.state = AssetState::Failed;
			return;
		}

		json j;
		{
			std::ifstream f(metaPath(asset->m_Meta.sourcePath));
			j = json::parse(f);
		}

		asset->m_Sources.vertex = readFile(asset->m_Meta.sourcePath);
		asset->m_Sources.fragment = j.contains("fragPath") ? readFile(j["fragPath"].get<std::string>()) : "";
		asset->m_Sources.compute = j.contains("computePath") ? readFile(j["computePath"].get<std::string>()) : "";

		if (asset->m_Sources.vertex.empty())
		{
			FUFU_ERROR("ShaderAsset: empty vertex source '{}'", asset->m_Meta.sourcePath.string());
			asset->m_Meta.state = AssetState::Failed;
			return;
		}

		asset->m_Meta.state = AssetState::Loaded;
		FUFU_INFO("Shader loaded: '{}'", asset->m_Meta.sourcePath.string());
	}

	std::shared_ptr<TextureAsset> AssetManager::getTexture(const std::filesystem::path& path)
	{
		UUID uuid = registerAsset(path, AssetType::Texture);
		return getAsset<TextureAsset>(uuid);
	}

	std::shared_ptr<MeshAsset> AssetManager::getMesh(const std::filesystem::path& path)
	{
		UUID uuid = registerAsset(path, AssetType::Mesh);
		return getAsset<MeshAsset>(uuid);
	}

	std::shared_ptr<ShaderAsset> AssetManager::getShader(const std::filesystem::path& vertPath, const std::filesystem::path& fragPath, const std::filesystem::path& computePath)
	{
		UUID uuid = registerAsset(vertPath, AssetType::Shader);

		// Wirte fragment/compute in the .meta
		auto metaOpt = readMeta(vertPath);
		if (metaOpt.has_value())
		{
			json j;
			std::ifstream f(metaPath(vertPath));
			j = json::parse(f);
			if (!fragPath.empty())    j["fragPath"] = fragPath.string();
			if (!computePath.empty()) j["computePath"] = computePath.string();

			std::ofstream out(metaPath(vertPath));
			out << j.dump(4);
		}

		return getAsset<ShaderAsset>(uuid);
	}

	void AssetManager::unload(UUID uuid)
	{
		auto it = m_Pool.find(uuid);
		if (it == m_Pool.end()) return;

		it->second->m_Meta.state = AssetState::Unloaded;

		// Specific data...
		if (it->second->getType() == AssetType::Texture)
		{
			auto tex = std::dynamic_pointer_cast<TextureAsset>(it->second);
			if (tex->m_Data.pixels)
			{
				stbi_image_free(tex->m_Data.pixels);
				tex->m_Data.pixels = nullptr;
			}
			if (tex->m_Data.floatPixels)
			{
				stbi_image_free(tex->m_Data.floatPixels);
				tex->m_Data.floatPixels = nullptr;
			}
		}

		FUFU_TRACE("Asset unloaded: UUID {}", uuid.value());
	}

	void AssetManager::unloadAll()
	{
		for (auto&[uuid, asset] : m_Pool)
			unload(uuid);
	}

}