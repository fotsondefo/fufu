#include "depch.h"
#include "Renderer/Passes/FXAAPass.h"
#include "Renderer/ShaderUtils.h"

namespace Fufu
{

	static const char* s_QuadVertSrc = R"(
#version 430 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)";

	// ----------------------------------------------------------------
	// FXAA — post-process par détection de contraste de luminance.
	// Version simplifiée "maison" (pas le FXAA 3.11 de NVIDIA) : détecte les
	// bords via le contraste de luminance entre voisins directs, puis floute
	// le long de la direction dominante du gradient (approximée via les 4
	// voisins diagonaux) proportionnellement à ce contraste. Moins précis que
	// l'algorithme complet mais bien moins de code, et suffisant pour lisser
	// les crénelures d'un rendu sans jitter.
	// ----------------------------------------------------------------

	static const char* s_FXAAFragSrc = R"(
#version 430 core
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Texture;
uniform vec2  u_TexelSize;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec3 rgbCenter = texture(u_Texture, v_UV).rgb;

    vec2 t = u_TexelSize;
    vec3 rgbN  = texture(u_Texture, v_UV + vec2( 0.0, -t.y)).rgb;
    vec3 rgbS  = texture(u_Texture, v_UV + vec2( 0.0,  t.y)).rgb;
    vec3 rgbE  = texture(u_Texture, v_UV + vec2( t.x,  0.0)).rgb;
    vec3 rgbW  = texture(u_Texture, v_UV + vec2(-t.x,  0.0)).rgb;
    vec3 rgbNW = texture(u_Texture, v_UV + vec2(-t.x, -t.y)).rgb;
    vec3 rgbNE = texture(u_Texture, v_UV + vec2( t.x, -t.y)).rgb;
    vec3 rgbSW = texture(u_Texture, v_UV + vec2(-t.x,  t.y)).rgb;
    vec3 rgbSE = texture(u_Texture, v_UV + vec2( t.x,  t.y)).rgb;

    float lC = luma(rgbCenter);
    float lN = luma(rgbN),  lS = luma(rgbS),  lE = luma(rgbE),  lW = luma(rgbW);
    float lNW = luma(rgbNW), lNE = luma(rgbNE), lSW = luma(rgbSW), lSE = luma(rgbSE);

    float lMin = min(lC, min(min(lN, lS), min(lE, lW)));
    float lMax = max(lC, max(max(lN, lS), max(lE, lW)));
    float contrast = lMax - lMin;

    // Zone plate : pas de bord, on sort la couleur brute pour ne pas flouter
    // des détails fins pour rien.
    float threshold = max(0.0312, lMax * 0.125);
    if (contrast < threshold) {
        fragColor = vec4(rgbCenter, 1.0);
        return;
    }

    // Sobel simplifié : direction dominante du gradient (horizontal vs vertical).
    float edgeHoriz = abs(lNW + lN + lNE - lSW - lS - lSE);
    float edgeVert  = abs(lNW + lW + lSW - lNE - lE - lSE);
    bool  horizontal = edgeHoriz >= edgeVert;

    vec3 blurA = horizontal ? rgbN : rgbW;
    vec3 blurB = horizontal ? rgbS : rgbE;
    vec3 blended = (rgbCenter + blurA + blurB) / 3.0;

    // Force le mélange proportionnellement au contraste local : bord net =
    // lissage fort, bord faible = presque inchangé.
    float blendFactor = clamp(contrast * 4.0, 0.0, 1.0);
    fragColor = vec4(mix(rgbCenter, blended, blendFactor), 1.0);
}
)";

	void FXAAPass::init(int width, int height)
	{
		uint32_t vs = compileShader(GL_VERTEX_SHADER, s_QuadVertSrc);
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, s_FXAAFragSrc);
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
