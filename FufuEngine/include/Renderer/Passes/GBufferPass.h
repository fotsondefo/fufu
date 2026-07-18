#pragma once

#include "Renderer/GPUScene.h"
#include "Renderer/RasterUniforms.h"
#include "RHI/RHIContext.h"
#include "RHI/RHICommandList.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHITexture.h"
#include "RHI/RHIBuffer.h"
#include <glm/glm.hpp>

namespace Fufu
{
	class GBufferPass
	{
	public:
		void init   (RHI::RHIContext& ctx, int width, int height);
		void shutdown();
		void resize (RHI::RHIContext& ctx, int width, int height);

		void render(RHI::RHICommandList& cmd,
		            const GPUScene& gpu,
		            const GPUFrameUBO& frame,
		            int width, int height);

		RHI::RHITexture* getPositionTexture() const { return m_PositionTex.get(); }
		RHI::RHITexture* getNormalTexture()   const { return m_NormalTex.get();   }
		RHI::RHITexture* getUVTexture()       const { return m_UVTex.get();       }

	private:
		void createAttachments(RHI::RHIContext& ctx, int w, int h);

		RHI::RHIContext*           m_Ctx = nullptr;

		RHI::Ref<RHI::RHIPipeline> m_Pipeline;

		RHI::Ref<RHI::RHITexture>  m_PositionTex;
		RHI::Ref<RHI::RHITexture>  m_NormalTex;
		RHI::Ref<RHI::RHITexture>  m_UVTex;
		RHI::Ref<RHI::RHITexture>  m_DepthTex;

		RHI::Ref<RHI::RHIBuffer>   m_FrameUBO;
		RHI::Ref<RHI::RHIBuffer>   m_DrawUBO;
	};
}
