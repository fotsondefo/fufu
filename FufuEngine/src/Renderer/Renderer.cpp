#include "depch.h"
#include "Project/Scene/Scene.h"
#include "Renderer/Renderer.h"
#include "Renderer/RasterUniforms.h"
#include "Project/Components.h"
#include "Application/Application.h"
#include "Application/Profiler.h"
#include "RHI/RHITexture.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Fufu
{

	// ── Init / Shutdown ───────────────────────────────────────────────────────

	void Renderer::init(int width, int height)
	{
		m_Width  = width;
		m_Height = height;

		// Contexte RHI — GL 4.3, debug callback activé en debug
		m_RHIContext = RHI::RHIContext::create({ RHI::Backend::OpenGL, nullptr,
#ifdef NDEBUG
			false
#else
			true
#endif
		});

		createTextures();
		createQuad();

		m_GPUScene.init(*m_RHIContext);
		m_ComputePass.init();
		m_FXAAPass.init(width, height);
		m_GBufferPass.init(*m_RHIContext, width, height);
		m_ForwardPass.init(*m_RHIContext, width, height);
		m_DeferredPass.init(*m_RHIContext, width, height);

		FUFU_INFO("Renderer initialized ({}x{})", width, height);
	}

	void Renderer::shutdown()
	{
		glDeleteTextures(1, &m_OutputTexture);
		glDeleteTextures(1, &m_AccumTexture);
		glDeleteVertexArrays(1, &m_QuadVAO);
		glDeleteBuffers(1, &m_QuadVBO);

		m_GPUScene.shutdown();
		m_Skybox.shutdown();
		m_ComputePass.shutdown();
		m_FXAAPass.shutdown();
		m_GBufferPass.shutdown();
		m_ForwardPass.shutdown();
		m_DeferredPass.shutdown();

		m_RHIContext.reset();
	}

	void Renderer::createTextures()
	{
		auto makeTexture = [&](uint32_t& id)
		{
			glGenTextures(1, &id);
			glBindTexture(GL_TEXTURE_2D, id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
				m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		};
		makeTexture(m_OutputTexture);
		makeTexture(m_AccumTexture);
	}

	void Renderer::createQuad()
	{
		float quad[] = {
			-1.f, -1.f,  0.f, 0.f,
			 1.f, -1.f,  1.f, 0.f,
			 1.f,  1.f,  1.f, 1.f,
			-1.f, -1.f,  0.f, 0.f,
			 1.f,  1.f,  1.f, 1.f,
			-1.f,  1.f,  0.f, 1.f,
		};
		glGenVertexArrays(1, &m_QuadVAO);
		glGenBuffers(1, &m_QuadVBO);
		glBindVertexArray(m_QuadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);
	}

	// ── Render ────────────────────────────────────────────────────────────────

	void Renderer::renderScene(Scene& scene)
	{
		if (sceneNeedsUpdate(scene))
		{
			m_GPUScene.upload(scene);
			if (m_Settings.resetOnMove)
				resetAccumulation();
		}

		Entity cam = scene.getPrimaryCamera();
		if (!cam)
		{
			clearOutput();
			return;
		}

		auto& camTransform = cam.getComponent<TransformComponent>();
		auto& camComponent = cam.getComponent<CameraComponent>();

		glm::mat4 view    = glm::inverse(camTransform.getTransform());
		glm::vec3 forward = glm::normalize(-glm::vec3(view[0][2], view[1][2], view[2][2]));
		glm::vec3 right   = glm::normalize( glm::vec3(view[0][0], view[1][0], view[2][0]));
		glm::vec3 up      = glm::normalize( glm::vec3(view[0][1], view[1][1], view[2][1]));

		GPUCamera gpuCam;
		gpuCam.position    = glm::vec4(camTransform.position, 1.f);
		gpuCam.forward     = glm::vec4(forward, 0.f);
		gpuCam.right       = glm::vec4(right,   0.f);
		gpuCam.up          = glm::vec4(up,       0.f);
		gpuCam.fov         = camComponent.fov;
		gpuCam.aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
		gpuCam.nearPlane   = camComponent.nearPlane;

		auto& pm = Fufu::Application::get().getProjectManager();
		if (pm.hasProject())
			m_Skybox.update(scene.getEnvironment(), pm.getCurrentProject().getAssetManager());

		GPUFrameData frameData;
		frameData.frameIndex       = (m_Settings.mode == RenderMode::Accumulation) ? m_FrameIndex : 0;
		frameData.maxBounces       = m_Settings.maxBounces;
		frameData.samplesPerPixel  = m_Settings.samplesPerPixel;
		frameData.exposure         = m_Settings.exposure;
		frameData.width            = m_Width;
		frameData.height           = m_Height;
		frameData.triangleCount    = m_GPUScene.getInstanceCount();
		frameData.materialCount    = m_GPUScene.getMaterialCount();
		frameData.lightCount       = m_GPUScene.getLightCount();
		frameData.technique        = static_cast<int>(m_Settings.technique);
		frameData.aaMode           = static_cast<int>(m_Settings.aaMode);
		frameData.taaFrameIndex    = m_TAAFrameIndex;
		frameData.taaBlendFactor   = m_Settings.taaBlendFactor;
		frameData.hasSkybox        = m_Skybox.isActive() ? 1 : 0;
		frameData.skyboxIntensity  = scene.getEnvironment().skyboxIntensity;

		const float aspect          = static_cast<float>(m_Width) / static_cast<float>(m_Height);
		const bool  useFXAA         = (m_Settings.aaMode == AAMode::FXAA);
		const bool  hasSkybox       = m_Skybox.isActive();
		const float skyboxIntensity = scene.getEnvironment().skyboxIntensity;
		int drawCalls = 0;

		if (m_Settings.technique == RenderTechnique::Forward ||
		    m_Settings.technique == RenderTechnique::Deferred)
		{
			RHI::RHICommandList* cmd = m_RHIContext->beginFrame();

			glm::mat4 proj = glm::perspective(gpuCam.fov, aspect, gpuCam.nearPlane, 10000.f);
			glm::mat4 vp   = proj * glm::inverse(camTransform.getTransform());

			GPUFrameUBO frame{};
			frame.viewProj        = vp;
			frame.camPos          = glm::vec3(gpuCam.position);
			frame.camForward      = glm::vec3(gpuCam.forward);
			frame.camFov          = gpuCam.fov;
			frame.camRight        = glm::vec3(gpuCam.right);
			frame.camAspect       = aspect;
			frame.camUp           = glm::vec3(gpuCam.up);
			frame.exposure        = m_Settings.exposure;
			frame.lightCount      = m_GPUScene.getLightCount();
			frame.hasSkybox       = hasSkybox ? 1 : 0;
			frame.skyboxIntensity = skyboxIntensity;

			if (m_Settings.technique == RenderTechnique::Forward)
			{
				m_ForwardPass.render(*cmd, m_GPUScene, frame,
					m_QuadVAO, m_Skybox.getTextureID(), m_Width, m_Height);

				drawCalls = static_cast<int>(m_GPUScene.getInstanceCount()) + 1;

				if (useFXAA)
				{
					m_FXAAPass.execute(
						static_cast<uint32_t>(m_ForwardPass.getOutputTexture()->getNativeHandle()),
						m_QuadVAO, m_Width, m_Height);
					++drawCalls;
				}
			}
			else // Deferred
			{
				m_GBufferPass.render(*cmd, m_GPUScene, frame, m_Width, m_Height);
				drawCalls = static_cast<int>(m_GPUScene.getInstanceCount());

				m_DeferredPass.render(*cmd, m_GPUScene,
					m_GBufferPass.getPositionTexture(),
					m_GBufferPass.getNormalTexture(),
					m_GBufferPass.getUVTexture(),
					frame,
					m_QuadVAO, m_Skybox.getTextureID(), m_Width, m_Height);
				++drawCalls;

				if (useFXAA)
				{
					m_FXAAPass.execute(
						static_cast<uint32_t>(m_DeferredPass.getOutputTexture()->getNativeHandle()),
						m_QuadVAO, m_Width, m_Height);
					++drawCalls;
				}
			}

			m_RHIContext->endFrame();
		}
		else
		{
			// PathTracing / RayTracing : compute shader, pas encore migré vers le RHI
			m_ComputePass.execute(m_GPUScene, gpuCam, frameData, m_OutputTexture, m_AccumTexture,
				m_Skybox.getTextureID(), m_Width, m_Height);
			drawCalls = 1;

			if (useFXAA)
			{
				m_FXAAPass.execute(m_OutputTexture, m_QuadVAO, m_Width, m_Height);
				++drawCalls;
			}

			if (m_Settings.mode == RenderMode::Accumulation &&
				m_FrameIndex < m_Settings.maxAccumFrames)
				++m_FrameIndex;

			++m_TAAFrameIndex;
		}

		Profiler::get().setCounters(m_GPUScene.getTriangleCount(), m_GPUScene.getInstanceCount(), drawCalls);
	}

	uint32_t Renderer::getOutputTextureID() const
	{
		if (m_Settings.aaMode == AAMode::FXAA)
			return m_FXAAPass.getOutputTexture();
		switch (m_Settings.technique)
		{
		case RenderTechnique::Forward:
			if (auto* tex = m_ForwardPass.getOutputTexture())
				return static_cast<uint32_t>(tex->getNativeHandle());
			break;
		case RenderTechnique::Deferred:
			if (auto* tex = m_DeferredPass.getOutputTexture())
				return static_cast<uint32_t>(tex->getNativeHandle());
			break;
		default:
			break;
		}
		return m_OutputTexture;
	}

	// ── Resize / Reset ────────────────────────────────────────────────────────

	void Renderer::resize(int width, int height)
	{
		m_Width  = width;
		m_Height = height;

		glDeleteTextures(1, &m_OutputTexture);
		glDeleteTextures(1, &m_AccumTexture);
		createTextures();

		m_FXAAPass.resize(width, height);
		m_GBufferPass.resize(*m_RHIContext, width, height);
		m_ForwardPass.resize(*m_RHIContext, width, height);
		m_DeferredPass.resize(*m_RHIContext, width, height);
		resetAccumulation();
	}

	void Renderer::resetAccumulation()
	{
		m_FrameIndex    = 0;
		m_TAAFrameIndex = 0;
	}

	void Renderer::clearOutput()
	{
		std::size_t pixelCount = static_cast<std::size_t>(m_Width) * static_cast<std::size_t>(m_Height) * 4;
		if (pixelCount == 0) return;

		static std::vector<float> s_ClearBuffer;
		if (s_ClearBuffer.size() != pixelCount)
		{
			s_ClearBuffer.resize(pixelCount);
			for (std::size_t i = 0; i < pixelCount; i += 4)
			{
				s_ClearBuffer[i + 0] = 0.15f;
				s_ClearBuffer[i + 1] = 0.15f;
				s_ClearBuffer[i + 2] = 0.16f;
				s_ClearBuffer[i + 3] = 1.f;
			}
		}

		auto clearGLTex = [&](uint32_t id) {
			if (!id) return;
			glBindTexture(GL_TEXTURE_2D, id);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_FLOAT, s_ClearBuffer.data());
		};
		auto clearRHITex = [&](RHI::RHITexture* tex) {
			if (tex) clearGLTex(static_cast<uint32_t>(tex->getNativeHandle()));
		};

		clearGLTex(m_OutputTexture);
		clearGLTex(m_FXAAPass.getOutputTexture());
		clearRHITex(m_ForwardPass.getOutputTexture());
		clearRHITex(m_DeferredPass.getOutputTexture());
	}

	bool Renderer::exportImage(const std::filesystem::path& path) const
	{
		if (m_Width <= 0 || m_Height <= 0) return false;

		std::vector<float> pixels(static_cast<std::size_t>(m_Width) * static_cast<std::size_t>(m_Height) * 4);
		glBindTexture(GL_TEXTURE_2D, getOutputTextureID());
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());

		std::vector<unsigned char> bytes(pixels.size());
		for (std::size_t i = 0; i < pixels.size(); ++i)
			bytes[i] = static_cast<unsigned char>(std::clamp(pixels[i], 0.f, 1.f) * 255.f + 0.5f);

		stbi_flip_vertically_on_write(1);
		bool ok = stbi_write_png(path.string().c_str(), m_Width, m_Height, 4, bytes.data(), m_Width * 4) != 0;
		if (!ok)
			FUFU_ERROR("Renderer: failed to export image to '{}'", path.string());
		return ok;
	}

	bool Renderer::sceneNeedsUpdate(Scene& scene)
	{
		if (&scene != m_LastScene || scene.getVersion() != m_LastSceneVersion)
		{
			m_LastScene        = &scene;
			m_LastSceneVersion = scene.getVersion();
			return true;
		}
		return false;
	}

}
