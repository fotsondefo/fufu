#include "depch.h"
#include "Renderer/Passes/DofPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace Fufu
{

// ── Helpers ───────────────────────────────────────────────────────────────────

static uint32_t buildProg(const char* vert, const char* frag)
{
	uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource(vert));
	uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource(frag));
	uint32_t p  = linkProgram({ vs, fs });
	glDeleteShader(vs);
	glDeleteShader(fs);
	return p;
}

static uint32_t makeR8Tex(int w, int h)
{
	uint32_t id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return id;
}

static uint32_t makeFloatTex(int w, int h)
{
	uint32_t id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return id;
}

static uint32_t makeFBO(uint32_t tex, bool isFloat)
{
	uint32_t fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	if (!isFloat)
	{
		// R8 FBO requires an explicit draw buffer
		GLenum bufs = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &bufs);
	}
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		FUFU_ERROR("DofPass: FBO incomplet");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return fbo;
}

// ── Init / Shutdown ───────────────────────────────────────────────────────────

void DofPass::init(int width, int height)
{
	m_CoCProgram  = buildProg("FullscreenQuad.vert", "DofCoC.frag");
	m_BlurProgram = buildProg("FullscreenQuad.vert", "DofBlur.frag");
	createTextures(width, height);
}

void DofPass::shutdown()
{
	glDeleteProgram(m_CoCProgram);
	glDeleteProgram(m_BlurProgram);
	glDeleteTextures(1, &m_CoCTex);
	glDeleteTextures(1, &m_BlurTex);
	glDeleteFramebuffers(1, &m_CoCFBO);
	glDeleteFramebuffers(1, &m_BlurFBO);
	m_CoCProgram = m_BlurProgram = 0;
	m_CoCTex = m_BlurTex = 0;
	m_CoCFBO = m_BlurFBO = 0;
}

void DofPass::resize(int width, int height)
{
	glDeleteTextures(1, &m_CoCTex);
	glDeleteTextures(1, &m_BlurTex);
	glDeleteFramebuffers(1, &m_CoCFBO);
	glDeleteFramebuffers(1, &m_BlurFBO);
	m_CoCTex = m_BlurTex = 0;
	m_CoCFBO = m_BlurFBO = 0;
	createTextures(width, height);
}

void DofPass::createTextures(int w, int h)
{
	m_CoCTex  = makeR8Tex(w, h);
	m_BlurTex = makeFloatTex(w, h);
	m_CoCFBO  = makeFBO(m_CoCTex,  false);
	m_BlurFBO = makeFBO(m_BlurTex, true);
}

// ── Execute ───────────────────────────────────────────────────────────────────

uint32_t DofPass::execute(uint32_t hdrTex,
                           uint32_t gPosTex,
                           uint32_t quadVAO,
                           int width, int height,
                           const glm::vec3& camPos,
                           float focusDist,
                           float focusRange,
                           float maxBlurRadius,
                           int   samples)
{
	glBindVertexArray(quadVAO);
	glDisable(GL_DEPTH_TEST);

	// ── Pass 1: Circle of Confusion ──────────────────────────────────────────
	Profiler::get().beginGPU("DofCoC");

	glBindFramebuffer(GL_FRAMEBUFFER, m_CoCFBO);
	glViewport(0, 0, width, height);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_CoCProgram);
	glUniform1i(glGetUniformLocation(m_CoCProgram, "u_GPosition"), 0);
	glUniform3fv(glGetUniformLocation(m_CoCProgram, "u_CamPos"),    1, glm::value_ptr(camPos));
	glUniform1f (glGetUniformLocation(m_CoCProgram, "u_FocusDist"), focusDist);
	glUniform1f (glGetUniformLocation(m_CoCProgram, "u_FocusRange"),focusRange);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gPosTex);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	Profiler::get().endGPU("DofCoC");

	// ── Pass 2: Bokeh blur ────────────────────────────────────────────────────
	Profiler::get().beginGPU("DofBlur");

	glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO);
	glViewport(0, 0, width, height);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_BlurProgram);
	glUniform1i(glGetUniformLocation(m_BlurProgram, "u_HDR"),           0);
	glUniform1i(glGetUniformLocation(m_BlurProgram, "u_CoC"),           1);
	glUniform2f(glGetUniformLocation(m_BlurProgram, "u_TexelSize"),
	            1.f / static_cast<float>(width),
	            1.f / static_cast<float>(height));
	glUniform1f(glGetUniformLocation(m_BlurProgram, "u_MaxBlurRadius"), maxBlurRadius);
	glUniform1i(glGetUniformLocation(m_BlurProgram, "u_Samples"),
	            std::clamp(samples, 1, 32));

	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdrTex);
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_CoCTex);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	Profiler::get().endGPU("DofBlur");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(0);

	return m_BlurTex;
}

} // namespace Fufu
