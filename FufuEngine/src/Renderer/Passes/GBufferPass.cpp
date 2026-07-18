#include "depch.h"
#include "Renderer/Passes/GBufferPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"
#include <glm/gtc/type_ptr.hpp>

namespace Fufu
{

	void GBufferPass::init(int width, int height)
	{
		uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("GBuffer.vert"));
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("GBuffer.frag"));
		m_Program   = linkProgram({ vs, fs });
		glDeleteShader(vs);
		glDeleteShader(fs);

		m_LocViewProj     = glGetUniformLocation(m_Program, "u_ViewProj");
		m_LocTransform    = glGetUniformLocation(m_Program, "u_Transform");
		m_LocInvTransform = glGetUniformLocation(m_Program, "u_InvTransform");
		m_LocTriOffset    = glGetUniformLocation(m_Program, "u_TriOffset");
		m_LocMatIdx       = glGetUniformLocation(m_Program, "u_MaterialIndex");

		// VAO vide requis par le core profile même quand tous les données
		// viennent des SSBOs via gl_VertexID.
		glGenVertexArrays(1, &m_DummyVAO);

		createFBO(width, height);
	}

	void GBufferPass::shutdown()
	{
		glDeleteProgram(m_Program);
		glDeleteVertexArrays(1, &m_DummyVAO);
		deleteFBO();
	}

	void GBufferPass::resize(int width, int height)
	{
		deleteFBO();
		createFBO(width, height);
	}

	void GBufferPass::createFBO(int w, int h)
	{
		auto makeRGBA32F = [&](uint32_t& id)
		{
			glGenTextures(1, &id);
			glBindTexture(GL_TEXTURE_2D, id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		};
		makeRGBA32F(m_PositionTex);
		makeRGBA32F(m_NormalTex);
		makeRGBA32F(m_UVTex);

		glGenRenderbuffers(1, &m_DepthRBO);
		glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

		glGenFramebuffers(1, &m_FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_PositionTex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_NormalTex,   0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_UVTex,       0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_DepthRBO);

		const GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		glDrawBuffers(3, drawBufs);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			FUFU_ERROR("GBufferPass: framebuffer incomplet");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GBufferPass::deleteFBO()
	{
		glDeleteTextures(1, &m_PositionTex);
		glDeleteTextures(1, &m_NormalTex);
		glDeleteTextures(1, &m_UVTex);
		glDeleteRenderbuffers(1, &m_DepthRBO);
		glDeleteFramebuffers(1, &m_FBO);
		m_PositionTex = m_NormalTex = m_UVTex = m_DepthRBO = m_FBO = 0;
	}

	void GBufferPass::render(const GPUScene& gpu, const glm::mat4& viewProj, int width, int height)
	{
		const auto& instances  = gpu.getInstances();
		const auto& triCounts  = gpu.getInstanceTriCounts();
		if (instances.empty()) return;

		gpu.bind(); // binding 2=positions, 10=attributs, ...

		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glViewport(0, 0, width, height);

		// Effacer position (w=0 → ciel), normale, UV, depth
		const float clearPos[] = { 0.f, 0.f, 0.f, 0.f };
		const float clearNrm[] = { 0.f, 0.f, 0.f, 0.f };
		const float clearUV[]  = { 0.f, 0.f, 0.f, 0.f };
		glClearBufferfv(GL_COLOR, 0, clearPos);
		glClearBufferfv(GL_COLOR, 1, clearNrm);
		glClearBufferfv(GL_COLOR, 2, clearUV);
		glClear(GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glUseProgram(m_Program);
		glBindVertexArray(m_DummyVAO);
		glUniformMatrix4fv(m_LocViewProj, 1, GL_FALSE, glm::value_ptr(viewProj));

		Profiler::get().beginGPU("GBufferPass");
		for (std::size_t i = 0; i < instances.size(); ++i)
		{
			const GPUInstance& inst = instances[i];
			int triCount = triCounts[i];
			if (triCount <= 0) continue;

			glUniformMatrix4fv(m_LocTransform,    1, GL_FALSE, glm::value_ptr(inst.transform));
			glUniformMatrix4fv(m_LocInvTransform, 1, GL_FALSE, glm::value_ptr(inst.invTransform));
			glUniform1i(m_LocTriOffset, inst.blasTriOffset);
			glUniform1i(m_LocMatIdx,    inst.materialIndex);
			glDrawArrays(GL_TRIANGLES, 0, triCount * 3);
		}
		Profiler::get().endGPU("GBufferPass");

		glBindVertexArray(0);
		glDisable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

}
