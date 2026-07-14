#include "depch.h"
#include "Project/Scene/Scene.h"
#include "Renderer/Renderer.h"
#include "Project/Components.h"
#include "Application/Application.h"
#include "Application/Profiler.h"
#include <algorithm>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Fufu
{

	// ----------------------------------------------------------------
	// Init / Shutdown
	// ----------------------------------------------------------------

	void Renderer::init(int width, int height)
	{
		m_Width = width;
		m_Height = height;

		createTextures();
		createQuad();

		m_GPUScene.init();
		m_ComputePass.init();
		m_FXAAPass.init(width, height);

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
			// pos       uv
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

	// ----------------------------------------------------------------
	// Render
	// ----------------------------------------------------------------

	void Renderer::renderScene(Scene& scene)
	{
		// Upload sc�ne si chang�e
		if (sceneNeedsUpdate(scene))
		{
			m_GPUScene.upload(scene);
			if (m_Settings.resetOnMove)
				resetAccumulation();
		}

		// Cam�ra
		Entity cam = scene.getPrimaryCamera();
		if (!cam)
		{
			// Sans cam�ra, rien � rendre : effacer la sortie plut�t que de
			// laisser l'image de la sc�ne pr�c�dente affich�e � l'�cran.
			clearOutput();
			return;
		}

		auto& camTransform = cam.getComponent<TransformComponent>();
		auto& camComponent = cam.getComponent<CameraComponent>();

		glm::mat4 view = glm::inverse(camTransform.getTransform());
		glm::vec3 forward = glm::normalize(-glm::vec3(view[0][2], view[1][2], view[2][2]));
		glm::vec3 right = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
		glm::vec3 up = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));

		GPUCamera gpuCam;
		gpuCam.position = glm::vec4(camTransform.position.x, camTransform.position.y, camTransform.position.z, 1.f);
		gpuCam.forward = glm::vec4(forward, 0.f);
		gpuCam.right = glm::vec4(right, 0.f);
		gpuCam.up = glm::vec4(up, 0.f);
		gpuCam.fov = camComponent.fov;
		gpuCam.aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
		gpuCam.nearPlane = camComponent.nearPlane;

		auto& pm = Fufu::Application::get().getProjectManager();
		if (pm.hasProject())
			m_Skybox.update(scene.getEnvironment(), pm.getCurrentProject().getAssetManager());

		// Frame data
		GPUFrameData frameData;
		frameData.frameIndex = (m_Settings.mode == RenderMode::Accumulation)
			? m_FrameIndex : 0;
		frameData.maxBounces = m_Settings.maxBounces;
		frameData.samplesPerPixel = m_Settings.samplesPerPixel;
		frameData.exposure = m_Settings.exposure;
		frameData.width = m_Width;
		frameData.height = m_Height;
		frameData.triangleCount = m_GPUScene.getInstanceCount();
		frameData.materialCount = m_GPUScene.getMaterialCount();
		frameData.lightCount = m_GPUScene.getLightCount();
		frameData.technique = static_cast<int>(m_Settings.technique);
		frameData.aaMode = static_cast<int>(m_Settings.aaMode);
		frameData.taaFrameIndex = m_TAAFrameIndex;
		frameData.taaBlendFactor = m_Settings.taaBlendFactor;
		frameData.hasSkybox = m_Skybox.isActive() ? 1 : 0;
		frameData.skyboxIntensity = scene.getEnvironment().skyboxIntensity;

		m_ComputePass.execute(m_GPUScene, gpuCam, frameData, m_OutputTexture, m_AccumTexture,
			m_Skybox.getTextureID(), m_Width, m_Height);

		int drawCalls = 1; // le dispatch compute
		if (m_Settings.aaMode == AAMode::FXAA)
		{
			m_FXAAPass.execute(m_OutputTexture, m_QuadVAO, m_Width, m_Height);
			++drawCalls;
		}

		Profiler::get().setCounters(m_GPUScene.getTriangleCount(), m_GPUScene.getInstanceCount(), drawCalls);

		// Incr�menter uniquement en accumulation et si pas � la limite
		if (m_Settings.mode == RenderMode::Accumulation &&
			m_FrameIndex < m_Settings.maxAccumFrames)
			++m_FrameIndex;

		// Contrairement � m_FrameIndex, le compteur TAA avance toujours : c'est
		// ce qui lui permet de lisser aussi en RenderMode::Realtime.
		++m_TAAFrameIndex;
	}

	// ----------------------------------------------------------------
	// Resize / Reset
	// ----------------------------------------------------------------

	void Renderer::resize(int width, int height)
	{
		m_Width = width;
		m_Height = height;

		glDeleteTextures(1, &m_OutputTexture);
		glDeleteTextures(1, &m_AccumTexture);
		createTextures();
		m_FXAAPass.resize(width, height);
		resetAccumulation();
	}

	void Renderer::resetAccumulation()
	{
		m_FrameIndex = 0;
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

		// Les deux textures potentiellement affichées (voir getOutputTextureID)
		// doivent être effacées, sinon FXAA continuerait d'afficher l'ancienne
		// scène alors que m_OutputTexture, lui, est bien effacé.
		glBindTexture(GL_TEXTURE_2D, m_OutputTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_FLOAT, s_ClearBuffer.data());

		glBindTexture(GL_TEXTURE_2D, m_FXAAPass.getOutputTexture());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_FLOAT, s_ClearBuffer.data());
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

		// La texture suit la convention OpenGL (ligne 0 = bas de l'image, voir
		// le calcul de `uv` dans le compute shader) alors qu'un PNG attend sa
		// ligne 0 en haut : flip vertical à l'écriture pour matcher ce que
		// l'utilisateur voit réellement dans le viewport.
		stbi_flip_vertically_on_write(1);
		bool ok = stbi_write_png(path.string().c_str(), m_Width, m_Height, 4, bytes.data(), m_Width * 4) != 0;

		if (!ok)
			FUFU_ERROR("Renderer: failed to export image to '{}'", path.string());

		return ok;
	}

	bool Renderer::sceneNeedsUpdate(Scene& scene)
	{
		// Reconstruire toute la géométrie GPU (BVH inclus) à chaque frame était
		// le vrai goulot d'étranglement sur les meshes un peu lourds (~50k
		// triangles) : ce n'est plus nécessaire que quand la scène a réellement
		// changé (Scene::markDirty, voir Scene.h/EntityImpl.h) ou qu'on vient
		// de basculer sur une autre scène active.
		if (&scene != m_LastScene || scene.getVersion() != m_LastSceneVersion)
		{
			m_LastScene = &scene;
			m_LastSceneVersion = scene.getVersion();
			return true;
		}
		return false;
	}

}
