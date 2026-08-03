#include "depch.h"
#include "Renderer/Passes/SSAOPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"
#include <random>
#include <glm/gtc/type_ptr.hpp>

namespace Fufu
{

// ── Helpers ──────────────────────────────────────────────────────────────────

static uint32_t buildProgram(const char* vert, const char* frag)
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

static uint32_t makeR8FBO(uint32_t tex)
{
	uint32_t fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		FUFU_ERROR("SSAOPass: FBO incomplet");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return fbo;
}

// ── Kernel / noise generation ─────────────────────────────────────────────────

void SSAOPass::generateKernel()
{
	std::uniform_real_distribution<float> rnd(0.f, 1.f);
	std::default_random_engine            rng(42u);

	for (int i = 0; i < 64; ++i)
	{
		glm::vec3 s(rnd(rng) * 2.f - 1.f,
		            rnd(rng) * 2.f - 1.f,
		            rnd(rng));                 // hemisphere z > 0
		s = glm::normalize(s);
		s *= rnd(rng);

		float scale = float(i) / 64.f;
		scale = 0.1f + scale * scale * 0.9f;  // concentrate near the origin
		m_Kernel[static_cast<size_t>(i)] = s * scale;
	}
}

void SSAOPass::generateNoise()
{
	std::uniform_real_distribution<float> rnd(-1.f, 1.f);
	std::default_random_engine            rng(13u);

	float noiseData[16 * 2];
	for (int i = 0; i < 16; ++i)
	{
		noiseData[i * 2 + 0] = rnd(rng);
		noiseData[i * 2 + 1] = rnd(rng);
	}

	glGenTextures(1, &m_NoiseTex);
	glBindTexture(GL_TEXTURE_2D, m_NoiseTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 4, 4, 0, GL_RG, GL_FLOAT, noiseData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

// ── Init / Shutdown ───────────────────────────────────────────────────────────

void SSAOPass::init(int width, int height)
{
	m_AOProgram   = buildProgram("FullscreenQuad.vert", "SSAO.frag");
	m_BlurProgram = buildProgram("FullscreenQuad.vert", "SSAOBlur.frag");

	generateKernel();
	generateNoise();
	createTextures(width, height);
}

void SSAOPass::shutdown()
{
	glDeleteProgram(m_AOProgram);
	glDeleteProgram(m_BlurProgram);
	glDeleteTextures(1, &m_AOTex);
	glDeleteTextures(1, &m_BlurTex);
	glDeleteTextures(1, &m_NoiseTex);
	glDeleteFramebuffers(1, &m_AOFBO);
	glDeleteFramebuffers(1, &m_BlurFBO);
	m_AOProgram = m_BlurProgram = 0;
	m_AOTex = m_BlurTex = m_NoiseTex = 0;
	m_AOFBO = m_BlurFBO = 0;
}

void SSAOPass::resize(int width, int height)
{
	glDeleteTextures(1, &m_AOTex);
	glDeleteTextures(1, &m_BlurTex);
	glDeleteFramebuffers(1, &m_AOFBO);
	glDeleteFramebuffers(1, &m_BlurFBO);
	m_AOTex = m_BlurTex = 0;
	m_AOFBO = m_BlurFBO = 0;
	createTextures(width, height);
}

void SSAOPass::createTextures(int w, int h)
{
	m_AOTex   = makeR8Tex(w, h);
	m_BlurTex = makeR8Tex(w, h);
	m_AOFBO   = makeR8FBO(m_AOTex);
	m_BlurFBO = makeR8FBO(m_BlurTex);
}

// ── Execute ───────────────────────────────────────────────────────────────────

uint32_t SSAOPass::execute(uint32_t gPosTex,
                            uint32_t gNrmTex,
                            uint32_t quadVAO,
                            int width, int height,
                            const glm::mat4& view,
                            const glm::mat4& proj,
                            float radius,
                            float bias,
                            float strength,
                            int   numSamples)
{
	Profiler::get().beginGPU("SSAOPass");

	// ── SSAO pass ────────────────────────────────────────────────────────────
	glBindFramebuffer(GL_FRAMEBUFFER, m_AOFBO);
	glViewport(0, 0, width, height);
	glClearColor(1.f, 1.f, 1.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(m_AOProgram);

	// Matrices
	glUniformMatrix4fv(glGetUniformLocation(m_AOProgram, "u_View"), 1, GL_FALSE,
	                   glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(m_AOProgram, "u_Proj"), 1, GL_FALSE,
	                   glm::value_ptr(proj));

	// Parameters
	int n = std::min(numSamples, 64);
	glUniform1f(glGetUniformLocation(m_AOProgram, "u_Radius"),     radius);
	glUniform1f(glGetUniformLocation(m_AOProgram, "u_Bias"),       bias);
	glUniform1f(glGetUniformLocation(m_AOProgram, "u_Strength"),   strength);
	glUniform1i(glGetUniformLocation(m_AOProgram, "u_NumSamples"), n);
	glUniform2f(glGetUniformLocation(m_AOProgram, "u_ScreenSize"),
	            static_cast<float>(width), static_cast<float>(height));

	// Kernel
	glUniform3fv(glGetUniformLocation(m_AOProgram, "u_Kernel"), 64,
	             glm::value_ptr(m_Kernel[0]));

	// Samplers
	glUniform1i(glGetUniformLocation(m_AOProgram, "u_GPosition"), 0);
	glUniform1i(glGetUniformLocation(m_AOProgram, "u_GNormal"),   1);
	glUniform1i(glGetUniformLocation(m_AOProgram, "u_Noise"),     2);

	glActiveTexture(GL_TEXTURE0);  glBindTexture(GL_TEXTURE_2D, gPosTex);
	glActiveTexture(GL_TEXTURE1);  glBindTexture(GL_TEXTURE_2D, gNrmTex);
	glActiveTexture(GL_TEXTURE2);  glBindTexture(GL_TEXTURE_2D, m_NoiseTex);

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	Profiler::get().endGPU("SSAOPass");

	// ── Blur pass ────────────────────────────────────────────────────────────
	Profiler::get().beginGPU("SSAOBlur");

	glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO);
	glClearColor(1.f, 1.f, 1.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_BlurProgram);
	glUniform1i(glGetUniformLocation(m_BlurProgram, "u_AO"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_AOTex);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	Profiler::get().endGPU("SSAOBlur");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(0);

	return m_BlurTex;
}

} // namespace Fufu
