#include "depch.h"
#include "Renderer/Passes/ToneMappingPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"

namespace Fufu
{

void ToneMappingPass::init(int width, int height)
{
	uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("FullscreenQuad.vert"));
	uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("ToneMapping.frag"));
	m_Program = linkProgram({ vs, fs });
	glDeleteShader(vs);
	glDeleteShader(fs);

	createResources(width, height);
}

void ToneMappingPass::shutdown()
{
	glDeleteProgram(m_Program);
	glDeleteTextures(1, &m_Texture);
	glDeleteFramebuffers(1, &m_FBO);
}

void ToneMappingPass::resize(int width, int height)
{
	glDeleteTextures(1, &m_Texture);
	glDeleteFramebuffers(1, &m_FBO);
	createResources(width, height);
}

void ToneMappingPass::createResources(int width, int height)
{
	glGenTextures(1, &m_Texture);
	glBindTexture(GL_TEXTURE_2D, m_Texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glGenFramebuffers(1, &m_FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		FUFU_ERROR("ToneMappingPass: framebuffer incomplete");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ToneMappingPass::execute(uint32_t sourceTexture,
                               uint32_t quadVAO,
                               int width, int height,
                               ToneMappingOperator op,
                               float gamma)
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glViewport(0, 0, width, height);

	glUseProgram(m_Program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sourceTexture);
	glUniform1i(glGetUniformLocation(m_Program, "u_Source"),   0);
	glUniform1i(glGetUniformLocation(m_Program, "u_Operator"), static_cast<int>(op));
	glUniform1f(glGetUniformLocation(m_Program, "u_Gamma"),    gamma);

	Profiler::get().beginGPU("ToneMappingPass");
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	Profiler::get().endGPU("ToneMappingPass");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}
