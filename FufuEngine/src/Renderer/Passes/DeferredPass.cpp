#include "depch.h"
#include "Renderer/Passes/DeferredPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"
#include <glm/gtc/type_ptr.hpp>

namespace Fufu
{

	void DeferredPass::init(int width, int height)
	{
		uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("FullscreenQuad.vert"));
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("DeferredLighting.frag"));
		m_Program   = linkProgram({ vs, fs });
		glDeleteShader(vs);
		glDeleteShader(fs);

		m_LocCamPos          = glGetUniformLocation(m_Program, "u_CamPos");
		m_LocCamForward      = glGetUniformLocation(m_Program, "u_CamForward");
		m_LocCamRight        = glGetUniformLocation(m_Program, "u_CamRight");
		m_LocCamUp           = glGetUniformLocation(m_Program, "u_CamUp");
		m_LocCamFov          = glGetUniformLocation(m_Program, "u_CamFov");
		m_LocCamAspect       = glGetUniformLocation(m_Program, "u_CamAspect");
		m_LocExposure        = glGetUniformLocation(m_Program, "u_Exposure");
		m_LocLightCount      = glGetUniformLocation(m_Program, "u_LightCount");
		m_LocHasSkybox       = glGetUniformLocation(m_Program, "u_HasSkybox");
		m_LocSkyboxIntensity = glGetUniformLocation(m_Program, "u_SkyboxIntensity");

		createFBO(width, height);
	}

	void DeferredPass::shutdown()
	{
		glDeleteProgram(m_Program);
		deleteFBO();
	}

	void DeferredPass::resize(int width, int height)
	{
		deleteFBO();
		createFBO(width, height);
	}

	void DeferredPass::createFBO(int w, int h)
	{
		glGenTextures(1, &m_OutputTex);
		glBindTexture(GL_TEXTURE_2D, m_OutputTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glGenFramebuffers(1, &m_FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_OutputTex, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			FUFU_ERROR("DeferredPass: framebuffer incomplet");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void DeferredPass::deleteFBO()
	{
		glDeleteTextures(1, &m_OutputTex);
		glDeleteFramebuffers(1, &m_FBO);
		m_OutputTex = m_FBO = 0;
	}

	void DeferredPass::render(const GPUScene& gpu,
	                           uint32_t gPosition,
	                           uint32_t gNormal,
	                           uint32_t gUV,
	                           uint32_t quadVAO,
	                           uint32_t skyboxTex,
	                           bool hasSkybox,
	                           float skyboxIntensity,
	                           const glm::vec3& camPos,
	                           const glm::vec3& camForward,
	                           const glm::vec3& camRight,
	                           const glm::vec3& camUp,
	                           float camFov,
	                           float camAspect,
	                           float exposure,
	                           int width, int height)
	{
		gpu.bind(); // SSBOs : 3=matériaux, 9=lumières, ...

		// Unité 0 : skybox (layout(binding=0))
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, skyboxTex);

		// Unités 1..16 : textures matériaux (layout(binding=1))
		const auto& matTextures = gpu.getMaterialTextures();
		for (int i = 0; i < static_cast<int>(matTextures.size()) && i < 16; ++i)
		{
			glActiveTexture(GL_TEXTURE1 + i);
			glBindTexture(GL_TEXTURE_2D, matTextures[i]);
		}

		// Unités 17/18/19 : G-Buffer (layout(binding=17/18/19))
		glActiveTexture(GL_TEXTURE17);
		glBindTexture(GL_TEXTURE_2D, gPosition);
		glActiveTexture(GL_TEXTURE18);
		glBindTexture(GL_TEXTURE_2D, gNormal);
		glActiveTexture(GL_TEXTURE19);
		glBindTexture(GL_TEXTURE_2D, gUV);

		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glViewport(0, 0, width, height);
		glDisable(GL_DEPTH_TEST);

		glUseProgram(m_Program);
		glUniform3fv(m_LocCamPos,          1, glm::value_ptr(camPos));
		glUniform3fv(m_LocCamForward,      1, glm::value_ptr(camForward));
		glUniform3fv(m_LocCamRight,        1, glm::value_ptr(camRight));
		glUniform3fv(m_LocCamUp,           1, glm::value_ptr(camUp));
		glUniform1f (m_LocCamFov,          camFov);
		glUniform1f (m_LocCamAspect,       camAspect);
		glUniform1f (m_LocExposure,        exposure);
		glUniform1i (m_LocLightCount,      gpu.getLightCount());
		glUniform1i (m_LocHasSkybox,       hasSkybox ? 1 : 0);
		glUniform1f (m_LocSkyboxIntensity, skyboxIntensity);

		Profiler::get().beginGPU("DeferredPass");
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		Profiler::get().endGPU("DeferredPass");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

}
