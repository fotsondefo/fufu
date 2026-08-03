#include "depch.h"
#include "Project/Assets/AssetManager.h"
#include "Application/Application.h"
#include <nlohmann/json.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <fstream>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <functional>

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
		// Synchronous: immediately switches to Loading, even before the job starts
		// — this prevents a concurrent getAsset<T>() call (next frame) from
		// triggering a second load.
		asset->m_Meta.state = AssetState::Loading;

		std::shared_ptr<Asset> assetPtr = asset;
		AssetType type = asset->getType();
		auto success = std::make_shared<bool>(false);

		Application::get().getJobSystem().submit(
			// Background thread: fills ONLY the CPU data of the asset
			// (pixels, submeshes, pre-warmed BVH) — never touches
			// OpenGL or m_Meta.state.
			[this, assetPtr, type, success]()
			{
				switch (type)
				{
				case AssetType::Texture:
				{
					auto tex = std::dynamic_pointer_cast<TextureAsset>(assetPtr);
					*success = loadTexture(tex);
					break;
				}
				case AssetType::Mesh:
				{
					auto mesh = std::dynamic_pointer_cast<MeshAsset>(assetPtr);
					*success = loadMesh(mesh);
					if (*success)
					{
						// Pre-warm the BVH for each LOD level in the background:
						// otherwise the first GPU upload referencing this mesh
						// would trigger the BVH build (SAH) on the main thread
						// — exactly the stall we are trying to avoid.
						mesh->getOrBuildBLAS(0);
						mesh->getOrBuildBLAS(1);
						mesh->getOrBuildBLAS(2);
					}
					break;
				}
				case AssetType::Shader:
				{
					auto shader = std::dynamic_pointer_cast<ShaderAsset>(assetPtr);
					*success = loadShader(shader);
					break;
				}
				default:
					break;
				}
			},
			// Main-thread callback: the ONLY place where m_Meta.state transitions
			// to Loaded/Failed — never read/written from two threads at once.
			[assetPtr, success]()
			{
				assetPtr->m_Meta.state = *success ? AssetState::Loaded : AssetState::Failed;

				// GPUScene::upload() only runs if Scene::markDirty() has been called
				// (see Renderer::sceneNeedsUpdate) — without this, a mesh that
				// finishes loading AFTER the entity was created (i.e. after the sole
				// markDirty() from its creation) would NEVER be picked up by the
				// renderer: GPUScene had ignored it while it was still loading, and
				// nothing would request a re-upload once ready. We do not know here
				// which scene(s) reference it, so we mark all scenes loaded in the
				// active project — negligible cost (one extra re-upload, proportional
				// to entities, not triangles).
				if (*success)
				{
					auto& pm = Application::get().getProjectManager();
					if (pm.hasProject())
					{
						for (auto& [name, scene] : pm.getCurrentProject().getSceneManager().getLoadedScenes())
							scene->markDirty();
					}
				}
			}
		);
	}

	bool AssetManager::loadTexture(std::shared_ptr<TextureAsset>& asset)
	{
		const std::string path = asset->m_Meta.sourcePath.string();

		std::string ext = asset->m_Meta.sourcePath.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		asset->m_Data.isHDR = (ext == ".hdr");

		// Vertical flip: useful for mesh textures (OpenGL UV convention, v=0
		// at the bottom), but would break an equirectangular HDRI —
		// sampleSky() assumes the first row of the image is the zenith
		// (v=0 -> dir.y=1), not the opposite.
		stbi_set_flip_vertically_on_load(!asset->m_Data.isHDR);

		if (asset->m_Data.isHDR)
		{
			// Equirectangular HDRI (skybox): floating-point data, no 0-1 clamp
			// like classic 8-bit.
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
				return false;
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
				return false;
			}
		}

		FUFU_INFO("Texture loaded: '{}' ({}x{} ch:{}{})",
			path, asset->m_Data.width, asset->m_Data.height, asset->m_Data.channels,
			asset->m_Data.isHDR ? ", HDR" : "");
		return true;
	}

	bool AssetManager::loadMesh(std::shared_ptr<MeshAsset>& asset)
	{
		const std::string path = asset->m_Meta.sourcePath.string();

		Assimp::Importer importer;
		// AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY = 1.0 means "target scale = 1 unit = 1 metre".
		// aiProcess_GlobalScale reads the UnitScaleFactor embedded in FBX metadata
		// (Blender FBX export uses 100 = centimetres) and applies 1/100 automatically,
		// so a 1 m cube in Blender arrives as 1 unit here — matching Blender's viewport.
		// For GLTF/OBJ the metadata is absent or 1.0, so nothing changes for those.
		importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_Triangulate       |
			aiProcess_GenNormals        |
			aiProcess_CalcTangentSpace  |
			aiProcess_FlipUVs           |
			aiProcess_LimitBoneWeights  |
			aiProcess_GlobalScale
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			FUFU_ERROR("MeshAsset: Assimp error for '{}': {}", path, importer.GetErrorString());
			return false;
		}

		// ── Collect all unique bones across all meshes ────────────────────────

		// rawBoneData: boneName -> aiBone* (first occurrence with offset matrix)
		std::unordered_map<std::string, const aiBone*> rawBoneData;
		for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
		{
			const aiMesh* aimesh = scene->mMeshes[m];
			for (unsigned int b = 0; b < aimesh->mNumBones; ++b)
			{
				std::string bname = aimesh->mBones[b]->mName.C_Str();
				if (!rawBoneData.count(bname))
					rawBoneData[bname] = aimesh->mBones[b];
			}
		}

		const bool hasBones = !rawBoneData.empty();
		asset->m_HasBones = hasBones;

		if (hasBones)
		{
			// Assign bone indices via DFS from root so parents always come before
			// children (topological order required by computeBoneMatrices).
			std::unordered_map<std::string, int>& boneMap = asset->m_Skeleton.boneIndexMap;
			std::vector<Bone>&                    bones   = asset->m_Skeleton.bones;

			std::function<void(const aiNode*)> assignIdx = [&](const aiNode* node)
			{
				std::string name = node->mName.C_Str();
				if (rawBoneData.count(name))
					boneMap[name] = static_cast<int>(bones.size());
				for (unsigned int c = 0; c < node->mNumChildren; ++c)
					assignIdx(node->mChildren[c]);
			};
			// First pass: reserve indices (DFS order)
			std::function<void(const aiNode*)> reserveIdx = [&](const aiNode* node)
			{
				std::string name = node->mName.C_Str();
				if (rawBoneData.count(name))
				{
					Bone b;
					b.name = name;
					const aiMatrix4x4& om = rawBoneData.at(name)->mOffsetMatrix;
					b.inverseBindMatrix = glm::mat4(
						om.a1, om.b1, om.c1, om.d1,
						om.a2, om.b2, om.c2, om.d2,
						om.a3, om.b3, om.c3, om.d3,
						om.a4, om.b4, om.c4, om.d4
					);
					b.parentIndex = -1;
					boneMap[name] = static_cast<int>(bones.size());
					bones.push_back(b);
				}
				for (unsigned int c = 0; c < node->mNumChildren; ++c)
					reserveIdx(node->mChildren[c]);
			};
			reserveIdx(scene->mRootNode);

			// Second pass: set parent indices
			std::function<void(const aiNode*, int)> setParents =
				[&](const aiNode* node, int parentBoneIdx)
			{
				std::string name = node->mName.C_Str();
				int myIdx = parentBoneIdx;
				if (boneMap.count(name))
				{
					bones[boneMap[name]].parentIndex = parentBoneIdx;
					myIdx = boneMap[name];
				}
				for (unsigned int c = 0; c < node->mNumChildren; ++c)
					setParents(node->mChildren[c], myIdx);
			};
			setParents(scene->mRootNode, -1);

			FUFU_INFO("MeshAsset: skeleton loaded ({} bones)", bones.size());
		}

		// ── Load submeshes ────────────────────────────────────────────────────

		auto addBoneWeight = [](Vertex& v, int boneIdx, float weight)
		{
			// Fill the first empty slot; if all 4 full, replace the smallest.
			for (int s = 0; s < 4; ++s)
			{
				if (v.boneWeights[s] == 0.f)
				{
					v.boneIndices[s] = boneIdx;
					v.boneWeights[s] = weight;
					return;
				}
			}
			int minS = 0;
			for (int s = 1; s < 4; ++s)
				if (v.boneWeights[s] < v.boneWeights[minS]) minS = s;
			if (weight > v.boneWeights[minS])
			{
				v.boneIndices[minS] = boneIdx;
				v.boneWeights[minS] = weight;
			}
		};

		for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
		{
			const aiMesh* aimesh = scene->mMeshes[m];
			SubMesh sub;
			sub.name = aimesh->mName.C_Str();

			sub.vertices.reserve(aimesh->mNumVertices);
			for (unsigned int v = 0; v < aimesh->mNumVertices; ++v)
			{
				Vertex vertex;
				vertex.position = { aimesh->mVertices[v].x, aimesh->mVertices[v].y, aimesh->mVertices[v].z };
				vertex.normal   = { aimesh->mNormals[v].x,  aimesh->mNormals[v].y,  aimesh->mNormals[v].z  };
				vertex.uv       = aimesh->mTextureCoords[0]
				                  ? glm::vec2(aimesh->mTextureCoords[0][v].x, aimesh->mTextureCoords[0][v].y)
				                  : glm::vec2(0.f);
				vertex.tangent  = aimesh->mTangents
				                  ? glm::vec3(aimesh->mTangents[v].x, aimesh->mTangents[v].y, aimesh->mTangents[v].z)
				                  : glm::vec3(0.f);
				sub.vertices.push_back(vertex);
			}

			// Bone weights
			if (hasBones)
			{
				for (unsigned int b = 0; b < aimesh->mNumBones; ++b)
				{
					const aiBone* bone = aimesh->mBones[b];
					std::string bname  = bone->mName.C_Str();
					auto it = asset->m_Skeleton.boneIndexMap.find(bname);
					if (it == asset->m_Skeleton.boneIndexMap.end()) continue;
					int boneIdx = it->second;
					for (unsigned int w = 0; w < bone->mNumWeights; ++w)
						addBoneWeight(sub.vertices[bone->mWeights[w].mVertexId],
						              boneIdx, bone->mWeights[w].mWeight);
				}
				// Normalize weights
				for (auto& v : sub.vertices)
				{
					float sum = v.boneWeights.x + v.boneWeights.y +
					            v.boneWeights.z + v.boneWeights.w;
					if (sum > 0.f) v.boneWeights /= sum;
				}
			}

			sub.indices.reserve(aimesh->mNumFaces * 3);
			for (unsigned int f = 0; f < aimesh->mNumFaces; ++f)
				for (unsigned int i = 0; i < aimesh->mFaces[f].mNumIndices; ++i)
					sub.indices.push_back(aimesh->mFaces[f].mIndices[i]);

			asset->m_SubMeshes.push_back(std::move(sub));
		}

		// ── Load animations ───────────────────────────────────────────────────

		if (hasBones && scene->mNumAnimations > 0)
		{
			asset->m_AnimationClips.reserve(scene->mNumAnimations);
			for (unsigned int a = 0; a < scene->mNumAnimations; ++a)
			{
				const aiAnimation* anim = scene->mAnimations[a];
				AnimationClip clip;
				clip.name        = anim->mName.C_Str();
				clip.duration    = static_cast<float>(anim->mDuration);
				clip.ticksPerSec = static_cast<float>(
					anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 25.0);

				for (unsigned int c = 0; c < anim->mNumChannels; ++c)
				{
					const aiNodeAnim* ch = anim->mChannels[c];
					std::string bname = ch->mNodeName.C_Str();
					auto it = asset->m_Skeleton.boneIndexMap.find(bname);
					if (it == asset->m_Skeleton.boneIndexMap.end()) continue;

					AnimationChannel channel;
					channel.boneIndex = it->second;

					for (unsigned int k = 0; k < ch->mNumPositionKeys; ++k)
					{
						const auto& key = ch->mPositionKeys[k];
						channel.posKeys.push_back({
							static_cast<float>(key.mTime),
							{ key.mValue.x, key.mValue.y, key.mValue.z }
						});
					}
					for (unsigned int k = 0; k < ch->mNumRotationKeys; ++k)
					{
						const auto& key = ch->mRotationKeys[k];
						channel.rotKeys.push_back({
							static_cast<float>(key.mTime),
							glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z)
						});
					}
					for (unsigned int k = 0; k < ch->mNumScalingKeys; ++k)
					{
						const auto& key = ch->mScalingKeys[k];
						channel.scaleKeys.push_back({
							static_cast<float>(key.mTime),
							{ key.mValue.x, key.mValue.y, key.mValue.z }
						});
					}
					clip.channels.push_back(std::move(channel));
				}
				asset->m_AnimationClips.push_back(std::move(clip));
			}
			FUFU_INFO("MeshAsset: {} animation(s) loaded", asset->m_AnimationClips.size());
		}

		FUFU_INFO("Mesh loaded: '{}' ({} submeshes{})", path, asset->m_SubMeshes.size(),
			hasBones ? ", skinned" : "");
		return true;
	}

	bool AssetManager::loadShader(std::shared_ptr<ShaderAsset>& asset)
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
			return false;
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
			return false;
		}

		FUFU_INFO("Shader loaded: '{}'", asset->m_Meta.sourcePath.string());
		return true;
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