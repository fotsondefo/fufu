#include "depch.h"
#include "Renderer/Passes/BloomPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"

namespace Fufu
{

// ── Helpers ───────────────────────────────────────────────────────────────────

static uint32_t makeFBO(uint32_t tex)
{
	uint32_t fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		FUFU_ERROR("BloomPass: FBO incomplete");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return fbo;
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
	glBindTexture(GL_TEXTURE_2D, 0);
	return id;
}

// ── Init / Shutdown ───────────────────────────────────────────────────────────

void BloomPass::init(int width, int height)
{
	{
		uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("FullscreenQuad.vert"));
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("BloomBright.frag"));
		m_BrightProgram = linkProgram({ vs, fs });
		glDeleteShader(vs); glDeleteShader(fs);
	}
	{
		uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("FullscreenQuad.vert"));
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("BloomBlur.frag"));
		m_BlurProgram = linkProgram({ vs, fs });
		glDeleteShader(vs); glDeleteShader(fs);
	}
	{
		uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("FullscreenQuad.vert"));
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("BloomComposite.frag"));
		m_CompositeProgram = linkProgram({ vs, fs });
		glDeleteShader(vs); glDeleteShader(fs);
	}

	createResources(width, height);
}

void BloomPass::shutdown()
{
	glDeleteProgram(m_BrightProgram);
	glDeleteProgram(m_BlurProgram);
	glDeleteProgram(m_CompositeProgram);

	uint32_t textures[] = { m_BrightTex, m_PingTex, m_PongTex, m_CompositeTex };
	glDeleteTextures(4, textures);

	uint32_t fbos[] = { m_BrightFBO, m_PingFBO, m_PongFBO, m_CompositeFBO };
	glDeleteFramebuffers(4, fbos);
}

void BloomPass::resize(int width, int height)
{
	uint32_t textures[] = { m_BrightTex, m_PingTex, m_PongTex, m_CompositeTex };
	glDeleteTextures(4, textures);

	uint32_t fbos[] = { m_BrightFBO, m_PingFBO, m_PongFBO, m_CompositeFBO };
	glDeleteFramebuffers(4, fbos);

	createResources(width, height);
}

void BloomPass::createResources(int width, int height)
{
	const int hw = std::max(1, width  / 2);
	const int hh = std::max(1, height / 2);

	m_BrightTex    = makeFloatTex(hw, hh);
	m_PingTex      = makeFloatTex(hw, hh);
	m_PongTex      = makeFloatTex(hw, hh);
	m_CompositeTex = makeFloatTex(width, height);

	m_BrightFBO    = makeFBO(m_BrightTex);
	m_PingFBO      = makeFBO(m_PingTex);
	m_PongFBO      = makeFBO(m_PongTex);
	m_CompositeFBO = makeFBO(m_CompositeTex);
}

// ── Execute ───────────────────────────────────────────────────────────────────

uint32_t BloomPass::execute(uint32_t hdrTex,
                             uint32_t quadVAO,
                             int width, int height,
                             float threshold, float knee,
                             float strength, int iterations)
{
	const int hw = std::max(1, width  / 2);
	const int hh = std::max(1, height / 2);

	Profiler::get().beginGPU("BloomPass");
	glBindVertexArray(quadVAO);

	// ── 1. Bright extract (half-resolution) ─────────────────────────────────
	glBindFramebuffer(GL_FRAMEBUFFER, m_BrightFBO);
	glViewport(0, 0, hw, hh);
	glUseProgram(m_BrightProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTex);
	glUniform1i  (glGetUniformLocation(m_BrightProgram, "u_Source"),    0);
	glUniform1f  (glGetUniformLocation(m_BrightProgram, "u_Threshold"), threshold);
	glUniform1f  (glGetUniformLocation(m_BrightProgram, "u_Knee"),      knee);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// ── 2. Ping-pong blur (half-resolution, N H+V iterations) ───────────────
	glUseProgram(m_BlurProgram);
	const float tw = 1.f / static_cast<float>(hw);
	const float th = 1.f / static_cast<float>(hh);
	const int   locDir  = glGetUniformLocation(m_BlurProgram, "u_Direction");
	const int   locSize = glGetUniformLocation(m_BlurProgram, "u_TexelSize");
	const int   locSrc  = glGetUniformLocation(m_BlurProgram, "u_Source");

	// First pass: from BrightTex (result of bright extract)
	uint32_t srcTex  = m_BrightTex;
	uint32_t dstFBO  = m_PingFBO;
	uint32_t dstAlt  = m_PongFBO;
	uint32_t dstTex  = m_PingTex;
	uint32_t dstAltT = m_PongTex;

	const int passes = std::clamp(iterations, 1, 4) * 2; // H+V per iteration
	for (int p = 0; p < passes; ++p)
	{
		const bool horizontal = (p % 2 == 0);
		glBindFramebuffer(GL_FRAMEBUFFER, dstFBO);
		glViewport(0, 0, hw, hh);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, srcTex);
		glUniform1i(locSrc, 0);
		glUniform2f(locSize, tw, th);
		glUniform2f(locDir,
			horizontal ? 1.f : 0.f,
			horizontal ? 0.f : 1.f);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Advance: the output of this pass becomes the source for the next.
		srcTex  = dstTex;
		std::swap(dstFBO,  dstAlt);
		std::swap(dstTex,  dstAltT);
	}
	// srcTex now points to the last blur result.

	// ── 3. Additive composite (full resolution) ──────────────────────────────
	glBindFramebuffer(GL_FRAMEBUFFER, m_CompositeFBO);
	glViewport(0, 0, width, height);
	glUseProgram(m_CompositeProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, srcTex);
	glUniform1i(glGetUniformLocation(m_CompositeProgram, "u_HDR"),      0);
	glUniform1i(glGetUniformLocation(m_CompositeProgram, "u_Bloom"),    1);
	glUniform1f(glGetUniformLocation(m_CompositeProgram, "u_Strength"), strength);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	Profiler::get().endGPU("BloomPass");

	return m_CompositeTex;
}

}
