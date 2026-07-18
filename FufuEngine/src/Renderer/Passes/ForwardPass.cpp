#include "depch.h"
#include "Renderer/Passes/ForwardPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"
#include <glm/gtc/type_ptr.hpp>

namespace Fufu
{

	void ForwardPass::init(int width, int height)
	{
		// Programme ciel : FullscreenQuad.vert + Sky.frag
		{
			uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("FullscreenQuad.vert"));
			uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("Sky.frag"));
			m_SkyProgram = linkProgram({ vs, fs });
			glDeleteShader(vs);
			glDeleteShader(fs);

			m_SkyLocCamForward      = glGetUniformLocation(m_SkyProgram, "u_CamForward");
			m_SkyLocCamRight        = glGetUniformLocation(m_SkyProgram, "u_CamRight");
			m_SkyLocCamUp           = glGetUniformLocation(m_SkyProgram, "u_CamUp");
			m_SkyLocCamFov          = glGetUniformLocation(m_SkyProgram, "u_CamFov");
			m_SkyLocCamAspect       = glGetUniformLocation(m_SkyProgram, "u_CamAspect");
			m_SkyLocHasSkybox       = glGetUniformLocation(m_SkyProgram, "u_HasSkybox");
			m_SkyLocSkyboxIntensity = glGetUniformLocation(m_SkyProgram, "u_SkyboxIntensity");
			m_SkyLocExposure        = glGetUniformLocation(m_SkyProgram, "u_Exposure");
		}

		// Programme géométrie : GBuffer.vert + Forward.frag
		{
			uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("GBuffer.vert"));
			uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("Forward.frag"));
			m_GeoProgram = linkProgram({ vs, fs });
			glDeleteShader(vs);
			glDeleteShader(fs);

			m_GeoLocViewProj     = glGetUniformLocation(m_GeoProgram, "u_ViewProj");
			m_GeoLocTransform    = glGetUniformLocation(m_GeoProgram, "u_Transform");
			m_GeoLocInvTransform = glGetUniformLocation(m_GeoProgram, "u_InvTransform");
			m_GeoLocTriOffset    = glGetUniformLocation(m_GeoProgram, "u_TriOffset");
			m_GeoLocMatIdx       = glGetUniformLocation(m_GeoProgram, "u_MaterialIndex");
			m_GeoLocCamPos       = glGetUniformLocation(m_GeoProgram, "u_CamPos");
			m_GeoLocExposure     = glGetUniformLocation(m_GeoProgram, "u_Exposure");
			m_GeoLocLightCount   = glGetUniformLocation(m_GeoProgram, "u_LightCount");
		}

		glGenVertexArrays(1, &m_DummyVAO);
		createFBO(width, height);
	}

	void ForwardPass::shutdown()
	{
		glDeleteProgram(m_SkyProgram);
		glDeleteProgram(m_GeoProgram);
		glDeleteVertexArrays(1, &m_DummyVAO);
		deleteFBO();
	}

	void ForwardPass::resize(int width, int height)
	{
		deleteFBO();
		createFBO(width, height);
	}

	void ForwardPass::createFBO(int w, int h)
	{
		glGenTextures(1, &m_OutputTex);
		glBindTexture(GL_TEXTURE_2D, m_OutputTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glGenRenderbuffers(1, &m_DepthRBO);
		glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

		glGenFramebuffers(1, &m_FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_OutputTex, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_DepthRBO);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			FUFU_ERROR("ForwardPass: framebuffer incomplet");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void ForwardPass::deleteFBO()
	{
		glDeleteTextures(1, &m_OutputTex);
		glDeleteRenderbuffers(1, &m_DepthRBO);
		glDeleteFramebuffers(1, &m_FBO);
		m_OutputTex = m_DepthRBO = m_FBO = 0;
	}

	void ForwardPass::render(const GPUScene& gpu,
	                          const glm::mat4& viewProj,
	                          const glm::vec3& camPos,
	                          const glm::vec3& camForward,
	                          const glm::vec3& camRight,
	                          const glm::vec3& camUp,
	                          float camFov,
	                          float camAspect,
	                          uint32_t quadVAO,
	                          uint32_t skyboxTex,
	                          bool hasSkybox,
	                          float skyboxIntensity,
	                          float exposure,
	                          int width, int height)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glViewport(0, 0, width, height);
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// ------ Passe ciel : pas de depth test ni d'écriture depth ------
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		glUseProgram(m_SkyProgram);
		glUniform3fv(m_SkyLocCamForward,     1, glm::value_ptr(camForward));
		glUniform3fv(m_SkyLocCamRight,       1, glm::value_ptr(camRight));
		glUniform3fv(m_SkyLocCamUp,          1, glm::value_ptr(camUp));
		glUniform1f (m_SkyLocCamFov,         camFov);
		glUniform1f (m_SkyLocCamAspect,      camAspect);
		glUniform1i (m_SkyLocHasSkybox,      hasSkybox ? 1 : 0);
		glUniform1f (m_SkyLocSkyboxIntensity, skyboxIntensity);
		glUniform1f (m_SkyLocExposure,        exposure);

		// Skybox texture → unité 0 (layout(binding=0) dans Sky.frag)
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, skyboxTex);

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);

		// ------ Passe géométrie : PBR forward ------
		const auto& instances = gpu.getInstances();
		const auto& triCounts = gpu.getInstanceTriCounts();
		if (instances.empty())
		{
			glBindVertexArray(0);
			glDisable(GL_DEPTH_TEST);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return;
		}

		gpu.bind(); // SSBOs : 2=positions, 3=matériaux, 9=lumières, 10=attributs, ...

		// Textures matériaux → unités 1..16 (layout(binding=1) dans Forward.frag)
		const auto& matTextures = gpu.getMaterialTextures();
		for (int i = 0; i < static_cast<int>(matTextures.size()) && i < 16; ++i)
		{
			glActiveTexture(GL_TEXTURE1 + i);
			glBindTexture(GL_TEXTURE_2D, matTextures[i]);
		}

		glUseProgram(m_GeoProgram);
		glUniformMatrix4fv(m_GeoLocViewProj, 1, GL_FALSE, glm::value_ptr(viewProj));
		glUniform3fv(m_GeoLocCamPos,   1, glm::value_ptr(camPos));
		glUniform1f (m_GeoLocExposure,    exposure);
		glUniform1i (m_GeoLocLightCount,  gpu.getLightCount());

		glBindVertexArray(m_DummyVAO);

		Profiler::get().beginGPU("ForwardPass");
		for (std::size_t i = 0; i < instances.size(); ++i)
		{
			const GPUInstance& inst = instances[i];
			int triCount = triCounts[i];
			if (triCount <= 0) continue;

			glUniformMatrix4fv(m_GeoLocTransform,    1, GL_FALSE, glm::value_ptr(inst.transform));
			glUniformMatrix4fv(m_GeoLocInvTransform, 1, GL_FALSE, glm::value_ptr(inst.invTransform));
			glUniform1i(m_GeoLocTriOffset, inst.blasTriOffset);
			glUniform1i(m_GeoLocMatIdx,    inst.materialIndex);
			glDrawArrays(GL_TRIANGLES, 0, triCount * 3);
		}
		Profiler::get().endGPU("ForwardPass");

		glBindVertexArray(0);
		glDisable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

}
