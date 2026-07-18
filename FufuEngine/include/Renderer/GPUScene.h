#pragma once

#include "GPUBuffers.h"
#include "BVH.h"
#include "Project/Scene/Scene.h"
#include "RHI/RHIContext.h"
#include "RHI/RHICommandList.h"
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

		// Bind les SSBOs via le CommandList RHI.
		// Slots : 2=positions, 3=materials, 6=blasNodes, 7=instances,
		//         8=tlasNodes, 9=lights, 10=attributs.
		void bind(RHI::RHICommandList& cmd) const;

		// Bind direct GL — conservé pour ComputePass (non encore migré).
		void bindGL() const;

		int getInstanceCount() const { return static_cast<int>(m_Instances.size()); }
		int getMaterialCount() const { return static_cast<int>(m_Materials.size()); }
		int getLightCount()    const { return static_cast<int>(m_Lights.size()); }
		int getTriangleCount() const { return static_cast<int>(m_TrianglePositions.size()); }

		const std::vector<uint32_t>& getMaterialTextures()  const { return m_ActiveMaterialTextures; }
		const std::vector<int>&      getInstanceTriCounts() const { return m_InstanceTriCounts; }

		const std::vector<GPUBVHNode>&           getBLASNodes()          const { return m_BLASNodes; }
		const std::vector<GPUBVHNode>&           getTLASNodes()          const { return m_TLASNodes; }
		const std::vector<GPUInstance>&          getInstances()          const { return m_Instances; }
		const std::vector<GPUTrianglePosition>&  getTrianglePositions()  const { return m_TrianglePositions; }
		const std::vector<GPUTriangleAttribute>& getTriangleAttributes() const { return m_TriangleAttributes; }

	private:
		int resolveAlbedoTexture(uint64_t textureUUID, AssetManager& assetManager,
			std::unordered_map<std::string, int>& frameSlots);

		// Recrée le buffer RHI si nécessaire (null ou trop petit), puis uploade.
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
		std::vector<GPUBVHNode>  m_TLASNodes;
		std::vector<GPULight>    m_Lights;
	};
}
