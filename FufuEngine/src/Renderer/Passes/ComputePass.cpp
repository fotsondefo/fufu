#include "depch.h"
#include "Renderer/Passes/ComputePass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"

namespace Fufu
{

	// ----------------------------------------------------------------
	// Compute shader — full path tracing (GI + shadows + reflections +
	// refraction). Source loaded from shaders/PathTracer.comp — see
	// ShaderUtils::loadShaderSource.
	// ----------------------------------------------------------------

	void ComputePass::init()
	{
		uint32_t cs = compileShader(GL_COMPUTE_SHADER, loadShaderSource("PathTracer.comp"));
		m_Program = linkProgram({ cs });
		glDeleteShader(cs);

		glGenBuffers(1, &m_CameraUBO);
		glGenBuffers(1, &m_FrameDataUBO);
	}

	void ComputePass::shutdown()
	{
		glDeleteProgram(m_Program);
		glDeleteBuffers(1, &m_CameraUBO);
		glDeleteBuffers(1, &m_FrameDataUBO);
	}

	void ComputePass::execute(const GPUScene& scene, const GPUCamera& camera, const GPUFrameData& frameData,
		uint32_t outputTexture, uint32_t accumTexture, uint32_t skyboxTexture, int width, int height)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_CameraUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUCamera), &camera, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_UNIFORM_BUFFER, m_FrameDataUBO);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUFrameData), &frameData, GL_DYNAMIC_DRAW);

		glUseProgram(m_Program);

		glBindImageTexture(0, outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindImageTexture(1, accumTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, skyboxTexture);

		// Material textures (albedo): units 1..kMaxMaterialTextures,
		// unit 0 being reserved for the skybox. Slots beyond the actually
		// active count are never read by the shader (see
		// GPUMaterial::albedoTexIdx), no need to clear them.
		const auto& materialTextures = scene.getMaterialTextures();
		for (std::size_t i = 0; i < materialTextures.size() && i < static_cast<std::size_t>(kMaxMaterialTextures); ++i)
		{
			glActiveTexture(GL_TEXTURE1 + static_cast<GLenum>(i));
			glBindTexture(GL_TEXTURE_2D, materialTextures[i]);
		}

		scene.bindGL();

		glBindBufferBase(GL_UNIFORM_BUFFER, 4, m_CameraUBO);
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, m_FrameDataUBO);

		// Dispatch in 16x16 groups
		int gx = (width + 15) / 16;
		int gy = (height + 15) / 16;

		Profiler::get().beginGPU("ComputePass");
		glDispatchCompute(gx, gy, 1);

		// Barrier before reading by a subsequent pass (FXAA) or by ImGui.
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		Profiler::get().endGPU("ComputePass");
	}

}
