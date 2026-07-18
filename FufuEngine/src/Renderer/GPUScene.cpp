#include "depch.h"
#include "Renderer/GPUScene.h"
#include "Project/Components.h"
#include "Project/Assets/AssetManager.h"
#include "Renderer/GroomGenerator.h"
#include "Application/Application.h"
#include "RHI/GL/GLResources.h" // pour bindGL() : static_cast<GLBuffer*>

namespace Fufu
{

	namespace
	{
		uint64_t hashBytes(const void* data, std::size_t size)
		{
			const unsigned char* bytes = static_cast<const unsigned char*>(data);
			uint64_t h = 1469598103934665603ull;
			for (std::size_t i = 0; i < size; ++i)
			{
				h ^= bytes[i];
				h *= 1099511628211ull;
			}
			return h;
		}
		uint64_t hashGroomParams(const GroomComponent& g) { return hashBytes(&g, sizeof(GroomComponent)); }
	}

	// ── Init / Shutdown ───────────────────────────────────────────────────────

	void GPUScene::init(RHI::RHIContext& ctx)
	{
		m_Context = &ctx;
		// Les buffers sont créés à la demande dans upload() via ensureBuffer().
	}

	void GPUScene::shutdown()
	{
		m_PositionBuffer.reset();
		m_AttributeBuffer.reset();
		m_MaterialBuffer.reset();
		m_BLASNodeBuffer.reset();
		m_InstanceBuffer.reset();
		m_TLASNodeBuffer.reset();
		m_LightBuffer.reset();

		for (auto& [path, texId] : m_MaterialTextureCache)
			glDeleteTextures(1, &texId);
		m_MaterialTextureCache.clear();
		m_Context = nullptr;
	}

	// ── Helpers ───────────────────────────────────────────────────────────────

	void GPUScene::ensureBuffer(RHI::Ref<RHI::RHIBuffer>& buf,
	                             const void* data, size_t byteSize,
	                             RHI::BufferUsage usage, RHI::MemoryType memory)
	{
		if (byteSize == 0) return;
		if (!buf || buf->getSize() < byteSize)
			buf = m_Context->createBuffer({ byteSize, usage, memory, data });
		else
			buf->upload(data, byteSize);
	}

	int GPUScene::resolveAlbedoTexture(uint64_t textureUUID, AssetManager& assetManager,
		std::unordered_map<std::string, int>& frameSlots)
	{
		if (textureUUID == 0) return -1;

		auto texAsset = assetManager.getAsset<TextureAsset>(UUID(textureUUID));
		if (!texAsset) return -1;

		std::string path = texAsset->getMeta().sourcePath.string();

		auto slotIt = frameSlots.find(path);
		if (slotIt != frameSlots.end())
			return slotIt->second;

		if (static_cast<int>(m_ActiveMaterialTextures.size()) >= kMaxMaterialTextures)
		{
			FUFU_WARN("GPUScene: texture limit reached ({}) for material textures, '{}' ignored", kMaxMaterialTextures, path);
			return -1;
		}

		auto cacheIt = m_MaterialTextureCache.find(path);
		uint32_t glTex;
		if (cacheIt != m_MaterialTextureCache.end())
		{
			glTex = cacheIt->second;
		}
		else
		{
			glGenTextures(1, &glTex);
			glBindTexture(GL_TEXTURE_2D, glTex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

			if (texAsset->isHDR())
			{
				GLenum format = (texAsset->getChannels() >= 4) ? GL_RGBA : GL_RGB;
				GLenum internalFormat = (texAsset->getChannels() >= 4) ? GL_RGBA32F : GL_RGB32F;
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texAsset->getWidth(), texAsset->getHeight(), 0,
					format, GL_FLOAT, texAsset->getFloatPixels());
			}
			else
			{
				GLenum format = (texAsset->getChannels() >= 4) ? GL_RGBA : (texAsset->getChannels() == 1 ? GL_RED : GL_RGB);
				GLenum internalFormat = (texAsset->getChannels() >= 4) ? GL_RGBA8 : (texAsset->getChannels() == 1 ? GL_R8 : GL_RGB8);
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texAsset->getWidth(), texAsset->getHeight(), 0,
					format, GL_UNSIGNED_BYTE, texAsset->getPixels());
			}

			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			m_MaterialTextureCache.emplace(path, glTex);
			FUFU_INFO("GPUScene: material texture loaded '{}' ({}x{})", path, texAsset->getWidth(), texAsset->getHeight());
		}

		int slot = static_cast<int>(m_ActiveMaterialTextures.size());
		m_ActiveMaterialTextures.push_back(glTex);
		frameSlots.emplace(std::move(path), slot);
		return slot;
	}

	// ── Upload ────────────────────────────────────────────────────────────────

	void GPUScene::upload(Scene& scene)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject()) return;
		auto& am = pm.getCurrentProject().getAssetManager();

		glm::vec3 cameraPos(0.f);
		if (Entity cam = scene.getPrimaryCamera())
			cameraPos = cam.getComponent<TransformComponent>().position;

		auto selectLOD = [](float distance) -> int
		{
			if (distance > 40.f) return 2;
			if (distance > 15.f) return 1;
			return 0;
		};

		// ── Étape 1 : détection des changements de géométrie ─────────────────

		struct NeededMesh  { std::shared_ptr<MeshAsset> asset; int lod; };
		struct NeededGroom { std::shared_ptr<MeshAsset> surfaceAsset; const GroomComponent* groom; uint64_t paramHash; };

		std::unordered_map<std::string, NeededMesh>  neededMeshes;
		std::unordered_map<std::string, NeededGroom> neededGrooms;
		bool geometryChanged = false;

		auto isCacheEntryStale = [&](const std::string& key, uint64_t sourceVersion)
		{
			auto it = m_BLASCache.find(key);
			return it == m_BLASCache.end() || it->second.sourceVersion != sourceVersion;
		};

		scene.each<TransformComponent, MeshComponent, MaterialComponent>(
			[&](entt::entity, TransformComponent& transform, MeshComponent& meshComp, MaterialComponent&)
		{
			auto asset = am.getMesh(meshComp.meshPath);
			if (!asset || asset->getSubMeshCount() == 0) return;
			float distance = glm::length(transform.position - cameraPos);
			int lod = selectLOD(distance);
			std::string key = meshComp.meshPath + "#lod" + std::to_string(lod);
			neededMeshes.emplace(key, NeededMesh{ asset, lod });
			if (isCacheEntryStale(key, asset->getBLASVersion())) geometryChanged = true;
		});

		scene.each<TransformComponent, MeshComponent, GroomComponent>(
			[&](entt::entity e, TransformComponent&, MeshComponent& meshComp, GroomComponent& groom)
		{
			auto surfaceAsset = am.getMesh(meshComp.meshPath);
			if (!surfaceAsset) return;
			std::string key = "groom#" + std::to_string(static_cast<uint32_t>(e));
			uint64_t paramHash = hashGroomParams(groom);
			neededGrooms.emplace(key, NeededGroom{ surfaceAsset, &groom, paramHash });
			if (isCacheEntryStale(key, paramHash)) geometryChanged = true;
		});

		if (!geometryChanged && neededMeshes.size() + neededGrooms.size() != m_BLASCache.size())
			geometryChanged = true;

		// ── Étape 2 : reconstruction de la géométrie si nécessaire ───────────

		if (geometryChanged)
		{
			m_TrianglePositions.clear();
			m_TriangleAttributes.clear();
			m_BLASNodes.clear();
			std::unordered_map<std::string, BLASRef> freshCache;

			auto append = [&](const std::string& key,
				const std::vector<GPUTrianglePosition>& positions,
				const std::vector<GPUTriangleAttribute>& attributes,
				const std::vector<GPUBVHNode>& nodes,
				uint64_t sourceVersion)
			{
				if (positions.empty()) return;
				BLASRef ref{
					static_cast<int>(m_BLASNodes.size()),
					static_cast<int>(m_TrianglePositions.size()),
					static_cast<int>(positions.size()),
					sourceVersion
				};
				m_BLASNodes.insert(m_BLASNodes.end(), nodes.begin(), nodes.end());
				m_TrianglePositions.insert(m_TrianglePositions.end(), positions.begin(), positions.end());
				m_TriangleAttributes.insert(m_TriangleAttributes.end(), attributes.begin(), attributes.end());
				freshCache.emplace(key, ref);
			};

			for (auto& [key, needed] : neededMeshes)
			{
				const MeshBLAS& blas = needed.asset->getOrBuildBLAS(needed.lod);
				append(key, blas.positions, blas.attributes, blas.nodes, needed.asset->getBLASVersion());
			}

			for (auto& [key, needed] : neededGrooms)
			{
				std::vector<GPUTriangle> groomTris;
				GroomGenerator::generate(*needed.surfaceAsset, *needed.groom, groomTris);
				std::vector<GPUBVHNode>             groomNodes;
				std::vector<GPUTrianglePosition>    groomPositions;
				std::vector<GPUTriangleAttribute>   groomAttributes;
				if (!groomTris.empty())
				{
					groomNodes = BVHBuilder::build(groomTris);
					splitTriangleBuffers(groomTris, groomPositions, groomAttributes);
				}
				append(key, groomPositions, groomAttributes, groomNodes, needed.paramHash);
			}

			m_BLASCache = std::move(freshCache);

			ensureBuffer(m_PositionBuffer,
				m_TrianglePositions.data(),
				m_TrianglePositions.size() * sizeof(GPUTrianglePosition),
				RHI::BufferUsage::Storage, RHI::MemoryType::GPU);

			ensureBuffer(m_AttributeBuffer,
				m_TriangleAttributes.data(),
				m_TriangleAttributes.size() * sizeof(GPUTriangleAttribute),
				RHI::BufferUsage::Storage, RHI::MemoryType::GPU);

			ensureBuffer(m_BLASNodeBuffer,
				m_BLASNodes.data(),
				m_BLASNodes.size() * sizeof(GPUBVHNode),
				RHI::BufferUsage::Storage, RHI::MemoryType::GPU);
		}

		// ── Étape 3 : instances / matériaux / lumières / TLAS ────────────────

		m_Materials.clear();
		m_Instances.clear();
		m_InstanceTriCounts.clear();
		m_ActiveMaterialTextures.clear();
		std::unordered_map<std::string, int> frameTextureSlots;

		scene.each<TransformComponent, MeshComponent, MaterialComponent>(
			[&](entt::entity, TransformComponent& transform, MeshComponent& meshComp, MaterialComponent& matComp)
		{
			auto assetIt = neededMeshes.find(meshComp.meshPath + "#lod" +
				std::to_string(selectLOD(glm::length(transform.position - cameraPos))));
			if (assetIt == neededMeshes.end()) return;
			auto blasIt = m_BLASCache.find(assetIt->first);
			if (blasIt == m_BLASCache.end()) return;

			int matIdx = static_cast<int>(m_Materials.size());
			GPUMaterial gpuMat;
			gpuMat.albedo       = matComp.albedo;
			gpuMat.metallic     = matComp.metallic;
			gpuMat.roughness    = matComp.roughness;
			gpuMat.emissive     = matComp.emissive;
			gpuMat.ior          = 1.5f;
			gpuMat.albedoTexIdx = resolveAlbedoTexture(matComp.albedoTexID, am, frameTextureSlots);
			m_Materials.push_back(gpuMat);

			GPUInstance inst{};
			inst.transform      = transform.getTransform();
			inst.invTransform   = glm::inverse(inst.transform);
			inst.materialIndex  = matIdx;
			inst.blasNodeOffset = blasIt->second.nodeOffset;
			inst.blasTriOffset  = blasIt->second.triOffset;
			m_Instances.push_back(inst);
			m_InstanceTriCounts.push_back(blasIt->second.triCount);
		});

		scene.each<TransformComponent, MeshComponent, GroomComponent>(
			[&](entt::entity e, TransformComponent& transform, MeshComponent&, GroomComponent& groom)
		{
			std::string key = "groom#" + std::to_string(static_cast<uint32_t>(e));
			auto blasIt = m_BLASCache.find(key);
			if (blasIt == m_BLASCache.end()) return;

			int matIdx = static_cast<int>(m_Materials.size());
			GPUMaterial gpuMat;
			gpuMat.albedo       = groom.color;
			gpuMat.metallic     = 0.f;
			gpuMat.roughness    = 0.9f;
			gpuMat.emissive     = 0.f;
			gpuMat.ior          = 1.4f;
			gpuMat.albedoTexIdx = -1;
			m_Materials.push_back(gpuMat);

			GPUInstance inst{};
			inst.transform      = transform.getTransform();
			inst.invTransform   = glm::inverse(inst.transform);
			inst.materialIndex  = matIdx;
			inst.blasNodeOffset = blasIt->second.nodeOffset;
			inst.blasTriOffset  = blasIt->second.triOffset;
			m_Instances.push_back(inst);
			m_InstanceTriCounts.push_back(blasIt->second.triCount);
		});

		m_Lights.clear();
		scene.each<TransformComponent, LightComponent>(
			[&](entt::entity, TransformComponent& transform, LightComponent& light)
		{
			GPULight gpuLight{};
			gpuLight.color  = glm::vec4(light.color, light.intensity);
			gpuLight.radius = light.radius;
			if (light.type == LightType::Directional)
			{
				glm::quat rotation = glm::quat(transform.rotation);
				glm::vec3 forward  = glm::normalize(rotation * glm::vec3(0.f, 0.f, -1.f));
				gpuLight.positionOrDirection = glm::vec4(-forward, 0.f);
				gpuLight.type = 0;
			}
			else
			{
				gpuLight.positionOrDirection = glm::vec4(transform.position, 0.f);
				gpuLight.type = 1;
			}
			m_Lights.push_back(gpuLight);
		});

		// ── TLAS ─────────────────────────────────────────────────────────────

		std::vector<glm::vec3> instBoundsMin(m_Instances.size());
		std::vector<glm::vec3> instBoundsMax(m_Instances.size());

		for (std::size_t i = 0; i < m_Instances.size(); ++i)
		{
			const GPUInstance& inst = m_Instances[i];
			const GPUBVHNode& root  = m_BLASNodes[static_cast<std::size_t>(inst.blasNodeOffset)];
			glm::vec3 lmin = glm::vec3(root.boundsMin);
			glm::vec3 lmax = glm::vec3(root.boundsMax);
			glm::vec3 worldMin(1e30f), worldMax(-1e30f);
			for (int c = 0; c < 8; ++c)
			{
				glm::vec3 corner(
					(c & 1) ? lmax.x : lmin.x,
					(c & 2) ? lmax.y : lmin.y,
					(c & 4) ? lmax.z : lmin.z);
				glm::vec3 worldCorner = glm::vec3(inst.transform * glm::vec4(corner, 1.f));
				worldMin = glm::min(worldMin, worldCorner);
				worldMax = glm::max(worldMax, worldCorner);
			}
			instBoundsMin[i] = worldMin;
			instBoundsMax[i] = worldMax;
		}

		std::vector<int> order;
		m_TLASNodes = BVHBuilder::buildFromBounds(instBoundsMin, instBoundsMax, order);

		std::vector<GPUInstance> reorderedInstances(m_Instances.size());
		std::vector<int>         reorderedTriCounts(m_InstanceTriCounts.size());
		for (std::size_t i = 0; i < order.size(); ++i)
		{
			reorderedInstances[i] = m_Instances[static_cast<std::size_t>(order[i])];
			reorderedTriCounts[i] = m_InstanceTriCounts[static_cast<std::size_t>(order[i])];
		}
		m_Instances         = std::move(reorderedInstances);
		m_InstanceTriCounts = std::move(reorderedTriCounts);

		ensureBuffer(m_MaterialBuffer,
			m_Materials.data(), m_Materials.size() * sizeof(GPUMaterial),
			RHI::BufferUsage::Storage, RHI::MemoryType::CPU_TO_GPU);

		ensureBuffer(m_InstanceBuffer,
			m_Instances.data(), m_Instances.size() * sizeof(GPUInstance),
			RHI::BufferUsage::Storage, RHI::MemoryType::CPU_TO_GPU);

		ensureBuffer(m_TLASNodeBuffer,
			m_TLASNodes.data(), m_TLASNodes.size() * sizeof(GPUBVHNode),
			RHI::BufferUsage::Storage, RHI::MemoryType::CPU_TO_GPU);

		ensureBuffer(m_LightBuffer,
			m_Lights.data(),
			std::max(m_Lights.size(), std::size_t(1)) * sizeof(GPULight),
			RHI::BufferUsage::Storage, RHI::MemoryType::CPU_TO_GPU);

		FUFU_TRACE("Scene uploaded: {} instances, {} unique BLAS triangles, {} materials (geometry {})",
			m_Instances.size(), m_TrianglePositions.size(), m_Materials.size(),
			geometryChanged ? "rebuilt" : "reused");
	}

	// ── Bind ──────────────────────────────────────────────────────────────────

	void GPUScene::bind(RHI::RHICommandList& cmd) const
	{
		if (m_PositionBuffer)  cmd.bindStorageBuffer(2,  m_PositionBuffer.get());
		if (m_MaterialBuffer)  cmd.bindStorageBuffer(3,  m_MaterialBuffer.get());
		if (m_BLASNodeBuffer)  cmd.bindStorageBuffer(6,  m_BLASNodeBuffer.get());
		if (m_InstanceBuffer)  cmd.bindStorageBuffer(7,  m_InstanceBuffer.get());
		if (m_TLASNodeBuffer)  cmd.bindStorageBuffer(8,  m_TLASNodeBuffer.get());
		if (m_LightBuffer)     cmd.bindStorageBuffer(9,  m_LightBuffer.get());
		if (m_AttributeBuffer) cmd.bindStorageBuffer(10, m_AttributeBuffer.get());
	}

	void GPUScene::bindGL() const
	{
		auto glBind = [](uint32_t slot, const RHI::Ref<RHI::RHIBuffer>& buf)
		{
			if (buf)
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot,
				                 static_cast<RHI::GLBuffer*>(buf.get())->getHandle());
		};
		glBind(2,  m_PositionBuffer);
		glBind(3,  m_MaterialBuffer);
		glBind(6,  m_BLASNodeBuffer);
		glBind(7,  m_InstanceBuffer);
		glBind(8,  m_TLASNodeBuffer);
		glBind(9,  m_LightBuffer);
		glBind(10, m_AttributeBuffer);
	}

}
