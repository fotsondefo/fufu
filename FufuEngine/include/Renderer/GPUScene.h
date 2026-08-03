#pragma once

#include "GPUBuffers.h"
#include "BVH.h"
#include "Project/Scene/Scene.h"
#include "RHI/RHIContext.h"
#include "RHI/RHICommandList.h"
#include <entt/entt.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Fufu
{
	class AssetManager;

	class GPUScene
	{
	public:
		void init(RHI::RHIContext& ctx);
		void shutdown();

		void upload(Scene& scene);

		// Binds the SSBOs via the RHI CommandList.
		// Slots: 2=positions, 3=materials, 6=blasNodes, 7=instances,
		//        8=tlasNodes, 9=lights, 10=attributes.
		void bind(RHI::RHICommandList& cmd) const;

		// Direct GL bind — kept for ComputePass (not yet migrated).
		void bindGL() const;

		// CPU-side skinning: re-skins all animated instances and re-uploads
		// the skinned position buffer (SSBO 12). Call every frame AFTER upload().
		void updateSkinning(Scene& scene);

		// GL handle of the skinned position buffer (SSBO 12).
		// Returns 0 until the first upload() with any geometry.
		uint64_t getSkinnedBufferHandle() const
		{
			return m_SkinnedBuffer ? m_SkinnedBuffer->getNativeHandle() : 0;
		}

		int getInstanceCount() const { return static_cast<int>(m_Instances.size()); }
		int getMaterialCount() const { return static_cast<int>(m_Materials.size()); }
		int getLightCount()    const { return static_cast<int>(m_Lights.size()); }
		int getTriangleCount() const { return static_cast<int>(m_TrianglePositions.size()); }

		const std::vector<uint32_t>& getMaterialTextures()  const { return m_ActiveMaterialTextures; }
		const std::vector<int>&      getInstanceTriCounts() const { return m_InstanceTriCounts; }

		// Per-instance world-space AABBs (parallel to getInstances()).
		// Used for CPU frustum culling in the raster passes.
		struct AABB { glm::vec3 min, max; };
		const std::vector<AABB>& getInstanceAABBs() const { return m_InstanceAABBs; }

		const std::vector<GPULight>&             getLights()             const { return m_Lights; }
		uint64_t getPositionBufferHandle() const
		{
			return m_PositionBuffer ? m_PositionBuffer->getNativeHandle() : 0;
		}

		const std::vector<GPUBVHNode>&           getBLASNodes()          const { return m_BLASNodes; }
		const std::vector<GPUBVHNode>&           getTLASNodes()          const { return m_TLASNodes; }
		const std::vector<GPUInstance>&          getInstances()          const { return m_Instances; }
		const std::vector<GPUTrianglePosition>&  getTrianglePositions()  const { return m_TrianglePositions; }
		const std::vector<GPUTriangleAttribute>& getTriangleAttributes() const { return m_TriangleAttributes; }

	private:
		// Registers a texture UUID in the shared per-frame texture pool and returns
		// its slot index (into u_MaterialTextures[]), or -1 if the UUID is 0,
		// the asset is not ready, or the pool is full.
		int resolveTexture(uint64_t textureUUID, AssetManager& assetManager,
			std::unordered_map<std::string, int>& frameSlots);

		// Recreates the RHI buffer if needed (null or too small), then uploads.
		void ensureBuffer(RHI::Ref<RHI::RHIBuffer>& buf,
		                  const void* data, size_t byteSize,
		                  RHI::BufferUsage usage, RHI::MemoryType memory);

		struct BLASRef
		{
			int nodeOffset;
			int triOffset;
			int triCount;
			uint64_t sourceVersion;
		};

		std::unordered_map<std::string, BLASRef>    m_BLASCache;
		std::unordered_map<std::string, uint32_t>   m_MaterialTextureCache;
		std::vector<uint32_t> m_ActiveMaterialTextures;

		RHI::RHIContext* m_Context = nullptr;

		RHI::Ref<RHI::RHIBuffer> m_PositionBuffer;
		RHI::Ref<RHI::RHIBuffer> m_AttributeBuffer;
		RHI::Ref<RHI::RHIBuffer> m_MaterialBuffer;
		RHI::Ref<RHI::RHIBuffer> m_BLASNodeBuffer;
		RHI::Ref<RHI::RHIBuffer> m_InstanceBuffer;
		RHI::Ref<RHI::RHIBuffer> m_TLASNodeBuffer;
		RHI::Ref<RHI::RHIBuffer> m_LightBuffer;

		std::vector<GPUTrianglePosition>  m_TrianglePositions;
		std::vector<GPUTriangleAttribute> m_TriangleAttributes;
		std::vector<GPUMaterial> m_Materials;
		std::vector<GPUBVHNode>  m_BLASNodes;
		std::vector<GPUInstance> m_Instances;
		std::vector<int>         m_InstanceTriCounts;
		std::vector<AABB>        m_InstanceAABBs;   // parallel to m_Instances
		std::vector<GPUBVHNode>  m_TLASNodes;
		std::vector<GPULight>    m_Lights;

		// Skinning
		std::vector<GPUSkinTriangle>     m_SkinWeights;     // parallel to m_TrianglePositions
		std::vector<GPUTrianglePosition> m_SkinnedPositions; // scratch for CPU skinning
		RHI::Ref<RHI::RHIBuffer>        m_SkinnedBuffer;    // SSBO 12
		std::vector<entt::entity>        m_InstanceEntities; // entity per instance (parallel to m_Instances)
	};
}
