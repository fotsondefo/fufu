#include "depch.h"
#include "Renderer/Passes/FXAAPass.h"
#include "Renderer/ShaderUtils.h"

namespace Fufu
{

	// ----------------------------------------------------------------
	// FXAA — post-process par détection de contraste de luminance. Sources
	// chargées depuis shaders/FullscreenQuad.vert et shaders/FXAA.frag — voir
	// ShaderUtils::loadShaderSource, et FXAA.frag pour le détail de l'algo.
	// ----------------------------------------------------------------

	void FXAAPass::init(int width, int height)
	{
		uint32_t vs = compileShader(GL_VERTEX_SHADER, loadShaderSource("FullscreenQuad.vert"));
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("FXAA.frag"));
		m_Program = linkProgram({ vs, fs });
		glDeleteShader(vs);
		glDeleteShader(fs);

		createResources(width, height);
	}

	void FXAAPass::shutdown()
	{
		glDeleteProgram(m_Program);
		glDeleteTextures(1, &m_Texture);
		glDeleteFramebuffers(1, &m_FBO);
	}

	void FXAAPass::resize(int width, int height)
	{
		glDeleteTextures(1, &m_Texture);
		glDeleteFramebuffers(1, &m_FBO);
		createResources(width, height);
	}

	void FXAAPass::createResources(int width, int height)
	{
		glGenTextures(1, &m_Texture);
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glGenFramebuffers(1, &m_FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			FUFU_ERROR("FXAAPass: framebuffer incomplete");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void FXAAPass::execute(uint32_t sourceTexture, uint32_t quadVAO, int width, int height)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glViewport(0, 0, width, height);

		glUseProgram(m_Program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sourceTexture);
		glUniform1i(glGetUniformLocation(m_Program, "u_Texture"), 0);
		glUniform2f(glGetUniformLocation(m_Program, "u_TexelSize"),
			1.f / static_cast<float>(width), 1.f / static_cast<float>(height));

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

}
