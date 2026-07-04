#include "depch.h"
#include "Project/Scene/Scene.h"
#include "Renderer/Renderer.h"
#include "Project/Components.h"
#include "Application/Application.h"

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

		m_ComputePass.execute(m_GPUScene, gpuCam, frameData, m_OutputTexture, m_AccumTexture, m_Width, m_Height);

		if (m_Settings.aaMode == AAMode::FXAA)
			m_FXAAPass.execute(m_OutputTexture, m_QuadVAO, m_Width, m_Height);

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

	bool Renderer::sceneNeedsUpdate(Scene& /*scene*/)
	{
		// Pour l'instant on upload � chaque frame.
		// Tu peux ajouter un syst�me de version sur Scene plus tard
		// pour ne re-uploader que si des composants ont chang�.
		return true;
	}

}
