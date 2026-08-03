#include "depch.h"
#include "Renderer/Passes/ShadowPass.h"
#include "Renderer/ShaderUtils.h"
#include "Application/Profiler.h"

namespace Fufu
{

void ShadowPass::init()
{
	uint32_t vs = compileShader(GL_VERTEX_SHADER,   loadShaderSource("Shadow.vert"));
	uint32_t fs = compileShader(GL_FRAGMENT_SHADER, loadShaderSource("Shadow.frag"));
	m_Program = linkProgram({ vs, fs });
	glDeleteShader(vs);
	glDeleteShader(fs);

	// Dummy VAO: no attributes, but OpenGL requires a bound VAO.
	glGenVertexArrays(1, &m_VAO);

	// 2048x2048 depth texture with comparison mode for sampler2DShadow.
	glGenTextures(1, &m_DepthTexture);
	glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kSize, kSize, 0,
	             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float border[] = { 1.f, 1.f, 1.f, 1.f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &m_FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		FUFU_ERROR("ShadowPass: framebuffer incomplete");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Persistent UBOs: allocated once, updated each frame.
	glGenBuffers(1, &m_ShadowUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_ShadowUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

	glGenBuffers(1, &m_DrawUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_DrawUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUDrawUBO), nullptr, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void ShadowPass::shutdown()
{
	glDeleteProgram(m_Program);
	glDeleteVertexArrays(1, &m_VAO);
	glDeleteFramebuffers(1, &m_FBO);
	glDeleteTextures(1, &m_DepthTexture);
	glDeleteBuffers(1, &m_ShadowUBO);
	glDeleteBuffers(1, &m_DrawUBO);
}

void ShadowPass::execute(const GPUScene& gpu, const glm::mat4& lightSpaceMatrix)
{
	const auto& instances = gpu.getInstances();
	const auto& triCounts = gpu.getInstanceTriCounts();
	if (instances.empty()) return;

	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glViewport(0, 0, kSize, kSize);
	glClear(GL_DEPTH_BUFFER_BIT);

	glUseProgram(m_Program);
	glBindVertexArray(m_VAO);

	// Polygon offset: reduces shadow acne on lit surfaces.
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.f, 4.f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// ShadowBlock (binding 0) — lightSpaceMatrix, unchanged for all instances.
	glBindBuffer(GL_UNIFORM_BUFFER, m_ShadowUBO);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), &lightSpaceMatrix);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_ShadowUBO);

	// PositionBuffer SSBO (binding 2) — vertex pulling.
	auto posHandle = static_cast<GLuint>(gpu.getPositionBufferHandle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, posHandle);

	Profiler::get().beginGPU("ShadowPass");
	for (std::size_t i = 0; i < instances.size(); ++i)
	{
		int triCount = triCounts[i];
		if (triCount <= 0) continue;

		GPUDrawUBO draw{};
		draw.transform    = instances[i].transform;
		draw.invTransform = instances[i].invTransform;
		draw.triOffset    = instances[i].blasTriOffset;
		draw.materialIndex = instances[i].materialIndex;

		glBindBuffer(GL_UNIFORM_BUFFER, m_DrawUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GPUDrawUBO), &draw);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_DrawUBO);

		glDrawArrays(GL_TRIANGLES, 0, triCount * 3);
	}
	Profiler::get().endGPU("ShadowPass");

	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}
