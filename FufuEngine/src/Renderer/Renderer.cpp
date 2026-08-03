#include "depch.h"
#include "Project/Scene/Scene.h"
#include "Renderer/Renderer.h"
#include "Renderer/RasterUniforms.h"
#include "Renderer/Skeleton.h"
#include "Project/Components.h"
#include "Project/Assets/AssetManager.h"
#include "Application/Application.h"
#include "Application/Profiler.h"
#include "RHI/RHITexture.h"
#include <algorithm>
#include <chrono>
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

		// RHI context — GL 4.3, debug callback enabled in debug
		m_RHIContext = RHI::RHIContext::create({ RHI::Backend::OpenGL, nullptr,
#ifdef NDEBUG
			false
#else
			true
#endif
		});

		createTextures();
		createQuad();

		m_Skybox.init();
		m_GPUScene.init(*m_RHIContext);
		m_ComputePass.init();
		m_FXAAPass.init(width, height);
		m_ToneMappingPass.init(width, height);
		m_BloomPass.init(width, height);
		m_ShadowPass.init();
		m_SSAOPass.init(width, height);
		m_DofPass.init(width, height);
		m_VolumetricPass.init(width, height);
		m_VolumePass.init(width, height);
		m_GaussianSplatPass.init(width, height);
		m_GBufferPass.init(*m_RHIContext, width, height);
		m_ForwardPass.init(*m_RHIContext, width, height);
		m_DeferredPass.init(*m_RHIContext, width, height);

		initShaderWatcher();
		FUFU_INFO("Renderer initialized ({}x{})", width, height);
	}

	void Renderer::initShaderWatcher()
	{
		m_ShaderWatcher.clear();
		auto shaderDir = std::filesystem::current_path() / "shaders";

		auto watch = [&](const std::string& file, std::function<void()> cb)
		{
			m_ShaderWatcher.watch(shaderDir / file, std::move(cb));
		};

		// GBuffer pass
		auto reloadGBuffer = [this]() { m_GBufferPass.reloadShaders(*m_RHIContext); };
		watch("GBuffer.vert", reloadGBuffer);
		watch("GBuffer.frag", reloadGBuffer);

		// Forward pass
		auto reloadForward = [this]() { m_ForwardPass.reloadShaders(*m_RHIContext); };
		watch("Forward.frag",       reloadForward);
		watch("FullscreenQuad.vert", reloadForward);
		watch("Sky.frag",            reloadForward);

		// GBuffer.vert is shared: a change also needs to reload Forward
		m_ShaderWatcher.watch(shaderDir / "GBuffer.vert",
			[this]() { m_ForwardPass.reloadShaders(*m_RHIContext); });

		// DeferredLighting
		// (DeferredPass doesn't have reloadShaders yet — can be added later)
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
		m_ToneMappingPass.shutdown();
		m_BloomPass.shutdown();
		m_ShadowPass.shutdown();
		m_SSAOPass.shutdown();
		m_DofPass.shutdown();
		m_VolumetricPass.shutdown();
		m_VolumePass.shutdown();
		m_GaussianSplatPass.shutdown();
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

	// Advance every AnimatorComponent by dt and compute bone matrices.
	static void advanceAnimations(Scene& scene, float dt)
	{
		auto& pm = Application::get().getProjectManager();
		if (!pm.hasProject()) return;
		auto& am = pm.getCurrentProject().getAssetManager();

		scene.each<MeshComponent, AnimatorComponent>(
			[&](entt::entity, MeshComponent& mesh, AnimatorComponent& anim)
		{
			if (!anim.playing) return;

			auto meshAsset = am.getMesh(mesh.meshPath);
			if (!meshAsset || !meshAsset->hasBones()) return;
			const auto& clips = meshAsset->getAnimationClips();
			if (clips.empty() || anim.clipIndex < 0 ||
			    anim.clipIndex >= static_cast<int>(clips.size())) return;

			const AnimationClip& clip = clips[static_cast<std::size_t>(anim.clipIndex)];
			float ticksPerSec = std::max(clip.ticksPerSec, 0.001f);
			float duration    = clip.duration / ticksPerSec; // duration in seconds

			anim.time += dt * anim.speed;
			if (anim.loop && anim.time > duration)
				anim.time = std::fmod(anim.time, duration);
			else
				anim.time = std::min(anim.time, duration);

			float tickTime = anim.time * ticksPerSec;
			computeBoneMatrices(meshAsset->getSkeleton(), clip, tickTime,
			                    anim.currentBoneMatrices);
		});
	}

	void Renderer::renderScene(Scene& scene)
	{
		m_ShaderWatcher.poll();

		// Delta time for animation
		static auto s_LastTime = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		float dt  = std::chrono::duration<float>(now - s_LastTime).count();
		s_LastTime = now;
		dt = std::min(dt, 0.1f); // clamp to avoid big jumps after pauses

		if (sceneNeedsUpdate(scene))
		{
			m_GPUScene.upload(scene);
			if (m_Settings.resetOnMove)
				resetAccumulation();
		}

		// Advance skeletal animations (every frame, independent of scene dirty)
		advanceAnimations(scene, dt);
		m_GPUScene.updateSkinning(scene);

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
					m_QuadVAO, m_Skybox.getTextureID(),
					m_Skybox.getIrradianceMap(),
					m_Skybox.getPrefilteredMap(),
					m_Skybox.getBrdfLut(),
					m_Width, m_Height);

				drawCalls = static_cast<int>(m_GPUScene.getInstanceCount()) + 1;

				uint32_t hdrTex = static_cast<uint32_t>(
					m_ForwardPass.getOutputTexture()->getNativeHandle());

				if (m_Settings.bloomEnabled)
				{
					hdrTex = m_BloomPass.execute(hdrTex, m_QuadVAO,
						m_Width, m_Height,
						m_Settings.bloomThreshold, m_Settings.bloomKnee,
						m_Settings.bloomStrength,  m_Settings.bloomIterations);
					++drawCalls;
				}

				m_ToneMappingPass.execute(hdrTex, m_QuadVAO,
					m_Width, m_Height, m_Settings.tonemapping, m_Settings.gamma);
				++drawCalls;

				if (useFXAA)
				{
					m_FXAAPass.execute(m_ToneMappingPass.getOutputTexture(),
						m_QuadVAO, m_Width, m_Height);
					++drawCalls;
				}
			}
			else // Deferred
			{
				// Shadow pass — find the first directional light.
				GPUShadowUBO shadowUBO{};
				glm::mat4    lightSpaceMatrix(1.f);

				for (const auto& light : m_GPUScene.getLights())
				{
					if (light.type == 0) // Directional
					{
						glm::vec3 lightDir = -glm::normalize(glm::vec3(light.positionOrDirection));
						glm::vec3 up       = (glm::abs(lightDir.y) > 0.99f)
						                   ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
						glm::mat4 lv = glm::lookAt(
							frame.camPos - lightDir * 50.f, frame.camPos, up);
						glm::mat4 lp = glm::ortho(-30.f, 30.f, -30.f, 30.f, 0.1f, 200.f);
						lightSpaceMatrix          = lp * lv;
						shadowUBO.lightSpaceMatrix = lightSpaceMatrix;
						shadowUBO.shadowBias       = m_Settings.shadowBias;
						shadowUBO.shadowEnabled    = 1;
						break;
					}
				}

				if (shadowUBO.shadowEnabled)
					m_ShadowPass.execute(m_GPUScene, lightSpaceMatrix);

				uint32_t skinnedBuf = static_cast<uint32_t>(m_GPUScene.getSkinnedBufferHandle());
				m_GBufferPass.render(*cmd, m_GPUScene, frame, m_Width, m_Height, skinnedBuf);
				drawCalls = static_cast<int>(m_GPUScene.getInstanceCount());

				// SSAO (deferred only, after GBuffer)
				uint32_t gPosTex = static_cast<uint32_t>(
					m_GBufferPass.getPositionTexture()->getNativeHandle());
				uint32_t gNrmTex = static_cast<uint32_t>(
					m_GBufferPass.getNormalTexture()->getNativeHandle());
				glm::mat4 viewMat  = glm::inverse(camTransform.getTransform());
				glm::mat4 projMat  = glm::perspective(gpuCam.fov, aspect, gpuCam.nearPlane, 10000.f);

				uint32_t aoTex = 0;
				if (m_Settings.ssaoEnabled)
				{
					aoTex = m_SSAOPass.execute(gPosTex, gNrmTex, m_QuadVAO,
						m_Width, m_Height,
						viewMat, projMat,
						m_Settings.ssaoRadius,
						m_Settings.ssaoBias,
						m_Settings.ssaoStrength,
						m_Settings.ssaoSamples);
					++drawCalls;
				}

				m_DeferredPass.render(*cmd, m_GPUScene,
					m_GBufferPass.getPositionTexture(),
					m_GBufferPass.getNormalTexture(),
					m_GBufferPass.getUVTexture(),
					frame,
					shadowUBO, m_ShadowPass.getDepthTexture(),
					aoTex, m_Settings.ssaoEnabled,
					m_QuadVAO, m_Skybox.getTextureID(),
					m_Skybox.getIrradianceMap(),
					m_Skybox.getPrefilteredMap(),
					m_Skybox.getBrdfLut(),
					m_Width, m_Height);
				++drawCalls;

				uint32_t hdrTex = static_cast<uint32_t>(
					m_DeferredPass.getOutputTexture()->getNativeHandle());

				// DoF: before bloom, on the linear HDR buffer.
				if (m_Settings.dofEnabled)
				{
					hdrTex = m_DofPass.execute(hdrTex, gPosTex,
						m_QuadVAO, m_Width, m_Height,
						frame.camPos,
						m_Settings.dofFocusDist,
						m_Settings.dofFocusRange,
						m_Settings.dofMaxBlur,
						m_Settings.dofSamples);
					++drawCalls;
				}

				// Heterogeneous volumes (smoke, fire, clouds) — after DoF, before atmospheric fog.
				{
					glm::vec3 volLightDir(0.f, 1.f, 0.f);
					glm::vec3 volLightColor(1.f);
					float     volLightIntensity = 1.f;
					for (const auto& light : m_GPUScene.getLights())
					{
						if (light.type == 0)
						{
							volLightDir       = glm::normalize(glm::vec3(light.positionOrDirection));
							volLightColor     = glm::vec3(light.color);
							volLightIntensity = light.color.a;
							break;
						}
					}

					hdrTex = m_VolumePass.execute(scene,
						hdrTex, gPosTex,
						m_ShadowPass.getDepthTexture(),
						m_QuadVAO, m_Width, m_Height,
						frame.camPos,
						frame.camForward, frame.camRight, frame.camUp,
						frame.camFov, frame.camAspect,
						lightSpaceMatrix,
						m_Settings.shadowBias, shadowUBO.shadowEnabled != 0,
						volLightDir, volLightColor, volLightIntensity);
				}

				// 3D Gaussian Splatting — after volumes, before atmospheric fog.
				{
					glm::mat4 viewMat4 = glm::inverse(camTransform.getTransform());
					glm::mat4 projMat4 = glm::perspective(
						gpuCam.fov, aspect, gpuCam.nearPlane, 10000.f);
					hdrTex = m_GaussianSplatPass.execute(
						scene, hdrTex, gPosTex,
						m_QuadVAO, m_Width, m_Height,
						viewMat4, projMat4,
						frame.camPos, frame.camFov, frame.camAspect);
				}

				// Atmospheric volumetrics: after DoF, before Bloom.
				if (m_Settings.volEnabled)
				{
					// Extract color/direction from the first directional light.
					glm::vec3 volLightDir(0.f, 1.f, 0.f);
					glm::vec3 volLightColor(1.f);
					float     volLightIntensity = 1.f;
					for (const auto& light : m_GPUScene.getLights())
					{
						if (light.type == 0)
						{
							volLightDir       = glm::normalize(glm::vec3(light.positionOrDirection));
							volLightColor     = glm::vec3(light.color);
							volLightIntensity = light.color.a;
							break;
						}
					}

					hdrTex = m_VolumetricPass.execute(hdrTex, gPosTex,
						m_ShadowPass.getDepthTexture(),
						m_QuadVAO, m_Width, m_Height,
						frame.camPos,
						frame.camForward, frame.camRight, frame.camUp,
						frame.camFov, frame.camAspect,
						lightSpaceMatrix,
						m_Settings.shadowBias, shadowUBO.shadowEnabled != 0,
						volLightDir, volLightColor, volLightIntensity,
						m_Settings.volSteps,
						m_Settings.volDensity,
						m_Settings.volScattering,
						m_Settings.volAnisotropy,
						m_Settings.volAmbient,
						m_Settings.volMaxDist);
					++drawCalls;
				}

				if (m_Settings.bloomEnabled)
				{
					hdrTex = m_BloomPass.execute(hdrTex, m_QuadVAO,
						m_Width, m_Height,
						m_Settings.bloomThreshold, m_Settings.bloomKnee,
						m_Settings.bloomStrength,  m_Settings.bloomIterations);
					++drawCalls;
				}

				m_ToneMappingPass.execute(hdrTex, m_QuadVAO,
					m_Width, m_Height, m_Settings.tonemapping, m_Settings.gamma);
				++drawCalls;

				if (useFXAA)
				{
					m_FXAAPass.execute(m_ToneMappingPass.getOutputTexture(),
						m_QuadVAO, m_Width, m_Height);
					++drawCalls;
				}
			}

			m_RHIContext->endFrame();
		}
		else
		{
			// PathTracing / RayTracing: compute shader, not yet migrated to the RHI.
			// Output in m_OutputTexture (linear HDR after PathTracer.comp patch).
			m_ComputePass.execute(m_GPUScene, gpuCam, frameData, m_OutputTexture, m_AccumTexture,
				m_Skybox.getTextureID(), m_Width, m_Height);
			drawCalls = 1;

			uint32_t hdrTex = m_OutputTexture;
			if (m_Settings.bloomEnabled)
			{
				hdrTex = m_BloomPass.execute(hdrTex, m_QuadVAO,
					m_Width, m_Height,
					m_Settings.bloomThreshold, m_Settings.bloomKnee,
					m_Settings.bloomStrength,  m_Settings.bloomIterations);
				++drawCalls;
			}

			m_ToneMappingPass.execute(hdrTex, m_QuadVAO,
				m_Width, m_Height, m_Settings.tonemapping, m_Settings.gamma);
			++drawCalls;

			if (useFXAA)
			{
				m_FXAAPass.execute(m_ToneMappingPass.getOutputTexture(),
					m_QuadVAO, m_Width, m_Height);
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
		// ToneMappingPass is always the last pass before the optional FXAA.
		if (uint32_t t = m_ToneMappingPass.getOutputTexture())
			return t;
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
		m_ToneMappingPass.resize(width, height);
		m_BloomPass.resize(width, height);
		m_SSAOPass.resize(width, height);
		m_DofPass.resize(width, height);
		m_VolumetricPass.resize(width, height);
		m_VolumePass.resize(width, height);
		m_GaussianSplatPass.resize(width, height);
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
		clearGLTex(m_ToneMappingPass.getOutputTexture());
		clearGLTex(m_BloomPass.getOutputTexture());
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
