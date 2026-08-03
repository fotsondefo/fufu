#include "depch.h"
#include "Renderer/Passes/VolumetricPass.h"
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

static uint32_t makeRGBATex(int w, int h, GLenum internalFmt, GLenum filter)
{
	uint32_t id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	GLenum fmt = (internalFmt == GL_RGBA16F || internalFmt == GL_RGBA32F) ? GL_RGBA : GL_RGBA;
	glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFmt),
	             w, h, 0, fmt, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	return id;
}

static uint32_t makeFBO(uint32_t tex)
{
	uint32_t fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		FUFU_ERROR("VolumetricPass: FBO incomplet");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return fbo;
}

// ── Init / Shutdown ───────────────────────────────────────────────────────────

void VolumetricPass::init(int width, int height)
{
	m_MarchProgram     = buildProg("FullscreenQuad.vert", "Volumetric.frag");
	m_CompositeProgram = buildProg("FullscreenQuad.vert", "VolumetricComposite.frag");
	createTextures(width, height);
}

void VolumetricPass::shutdown()
{
	glDeleteProgram(m_MarchProgram);
	glDeleteProgram(m_CompositeProgram);
	glDeleteTextures(1, &m_ScatterTex);
	glDeleteTextures(1, &m_CompositeTex);
	glDeleteFramebuffers(1, &m_ScatterFBO);
	glDeleteFramebuffers(1, &m_CompositeFBO);
	m_MarchProgram = m_CompositeProgram = 0;
	m_ScatterTex = m_CompositeTex = 0;
	m_ScatterFBO = m_CompositeFBO = 0;
}

void VolumetricPass::resize(int width, int height)
{
	glDeleteTextures(1, &m_ScatterTex);
	glDeleteTextures(1, &m_CompositeTex);
	glDeleteFramebuffers(1, &m_ScatterFBO);
	glDeleteFramebuffers(1, &m_CompositeFBO);
	m_ScatterTex = m_CompositeTex = 0;
	m_ScatterFBO = m_CompositeFBO = 0;
	createTextures(width, height);
}

void VolumetricPass::createTextures(int w, int h)
{
	int hw = std::max(1, w / 2);
	int hh = std::max(1, h / 2);
	// Scatter: half-resolution, GL_LINEAR for implicit upsampling in the composite
	m_ScatterTex   = makeRGBATex(hw, hh, GL_RGBA16F, GL_LINEAR);
	m_CompositeTex = makeRGBATex(w,  h,  GL_RGBA32F, GL_LINEAR);
	m_ScatterFBO   = makeFBO(m_ScatterTex);
	m_CompositeFBO = makeFBO(m_CompositeTex);
}

// ── Execute ───────────────────────────────────────────────────────────────────

uint32_t VolumetricPass::execute(uint32_t hdrTex,
                                  uint32_t gPosTex,
                                  uint32_t shadowDepthTex,
                                  uint32_t quadVAO,
                                  int width, int height,
                                  const glm::vec3& camPos,
                                  const glm::vec3& camForward,
                                  const glm::vec3& camRight,
                                  const glm::vec3& camUp,
                                  float camFov, float camAspect,
                                  const glm::mat4& lightSpaceMatrix,
                                  float shadowBias, bool shadowEnabled,
                                  const glm::vec3& lightDir,
                                  const glm::vec3& lightColor,
                                  float lightIntensity,
                                  int   steps,
                                  float density,
                                  float scattering,
                                  float anisotropy,
                                  float ambient,
                                  float maxDist)
{
	const int hw = std::max(1, width  / 2);
	const int hh = std::max(1, height / 2);

	glBindVertexArray(quadVAO);
	glDisable(GL_DEPTH_TEST);

	// ── Pass 1: Raymarching (half-resolution) ────────────────────────────────
	Profiler::get().beginGPU("VolumetricMarch");

	glBindFramebuffer(GL_FRAMEBUFFER, m_ScatterFBO);
	glViewport(0, 0, hw, hh);
	glClearColor(0.f, 0.f, 0.f, 1.f);   // transmittance = 1 by default (transparent sky)
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_MarchProgram);

	auto loc = [&](const char* n) { return glGetUniformLocation(m_MarchProgram, n); };

	// Textures
	glUniform1i(loc("u_GPosition"), 0);
	glUniform1i(loc("u_ShadowMap"), 1);
	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPosTex);
	// Shadow map: bound with compare mode enabled → sampler2DShadow
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, shadowDepthTex);

	// Camera
	glUniform3fv(loc("u_CamPos"),     1, glm::value_ptr(camPos));
	glUniform3fv(loc("u_CamForward"), 1, glm::value_ptr(camForward));
	glUniform3fv(loc("u_CamRight"),   1, glm::value_ptr(camRight));
	glUniform3fv(loc("u_CamUp"),      1, glm::value_ptr(camUp));
	glUniform1f (loc("u_CamFov"),     camFov);
	glUniform1f (loc("u_CamAspect"),  camAspect);

	// Shadow / light
	glUniformMatrix4fv(loc("u_LightSpaceMatrix"), 1, GL_FALSE,
	                   glm::value_ptr(lightSpaceMatrix));
	glUniform1f(loc("u_ShadowBias"),     shadowBias);
	glUniform1i(loc("u_ShadowEnabled"),  shadowEnabled ? 1 : 0);
	glUniform3fv(loc("u_LightDir"),      1, glm::value_ptr(lightDir));
	glUniform3fv(loc("u_LightColor"),    1, glm::value_ptr(lightColor));
	glUniform1f (loc("u_LightIntensity"),lightIntensity);

	// Volume
	glUniform1i(loc("u_Steps"),       std::clamp(steps, 4, 128));
	glUniform1f(loc("u_Density"),     density);
	glUniform1f(loc("u_Scattering"),  scattering);
	glUniform1f(loc("u_Anisotropy"),  anisotropy);
	glUniform1f(loc("u_Ambient"),     ambient);
	glUniform1f(loc("u_MaxDist"),     maxDist);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	Profiler::get().endGPU("VolumetricMarch");

	// ── Pass 2: Full-resolution composite ────────────────────────────────────
	Profiler::get().beginGPU("VolumetricComposite");

	glBindFramebuffer(GL_FRAMEBUFFER, m_CompositeFBO);
	glViewport(0, 0, width, height);
	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(m_CompositeProgram);
	glUniform1i(glGetUniformLocation(m_CompositeProgram, "u_HDR"),     0);
	glUniform1i(glGetUniformLocation(m_CompositeProgram, "u_Scatter"), 1);
	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, hdrTex);
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_ScatterTex);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	Profiler::get().endGPU("VolumetricComposite");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(0);

	return m_CompositeTex;
}

} // namespace Fufu
