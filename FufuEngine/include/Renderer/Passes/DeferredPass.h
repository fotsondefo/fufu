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
	class DeferredPass
	{
	public:
		void init   (RHI::RHIContext& ctx, int width, int height);
		void shutdown();
		void resize (RHI::RHIContext& ctx, int width, int height);

		void render(RHI::RHICommandList& cmd,
		            const GPUScene& gpu,
		            RHI::RHITexture* gPosition,
		            RHI::RHITexture* gNormal,
		            RHI::RHITexture* gUV,
		            const GPUFrameUBO& frame,
		            uint32_t quadVAO,
		            uint32_t skyboxTex,
		            int width, int height);

		RHI::RHITexture* getOutputTexture() const { return m_OutputTex.get(); }

	private:
		void createAttachments(RHI::RHIContext& ctx, int w, int h);

		RHI::RHIContext*           m_Ctx = nullptr;

		RHI::Ref<RHI::RHIPipeline> m_Pipeline;

		RHI::Ref<RHI::RHITexture>  m_OutputTex;

		RHI::Ref<RHI::RHIBuffer>   m_FrameUBO;
	};
}
