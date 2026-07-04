#pragma once

#include "RenderSettings.h"
#include "GPUBuffers.h"
#include "GPUScene.h"
#include "Passes/ComputePass.h"
#include "Passes/FXAAPass.h"
#include "Project/Scene/Scene.h"

namespace Fufu
{

	// Orchestrateur : possède les textures partagées entre passes et l'état
	// d'accumulation, construit la caméra/frame GPU à partir de la Scene/ECS
	// (seule partie qui reste ici plutôt que dans une passe, car elle dépend
	// des components), puis délègue le gros du travail à GPUScene (empaquetage
	// géométrie) et aux passes (ComputePass, FXAAPass).
	class Renderer
	{
	public:
		Renderer() = default;
		~Renderer() = default;

		void init(int width, int height);
		void shutdown();

		// Appel� chaque frame depuis Application::run()
		void renderScene(Scene& scene);

		// Redimensionnement viewport
		void resize(int width, int height);

		RenderSettings& getSettings() { return m_Settings; }

		// Remet l'accumulation � z�ro (ex: cam�ra boug�e)
		void resetAccumulation();

		int getAccumulatedFrames() const { return m_FrameIndex; }

		// Texture à afficher : la sortie du FXAAPass si le mode FXAA est actif
		// (post-processée), m_OutputTexture sinon.
		uint32_t getOutputTextureID() const
		{
			return (m_Settings.aaMode == AAMode::FXAA) ? m_FXAAPass.getOutputTexture() : m_OutputTexture;
		}

	private:
		// Init OpenGL
		void createTextures();
		void createQuad();

		bool sceneNeedsUpdate(Scene& scene);

		// Efface la texture de sortie (scène sans caméra primaire) : sinon
		// l'image de la scène précédente reste affichée telle quelle.
		void clearOutput();

	private:
		RenderSettings m_Settings;

		int m_Width = 0;
		int m_Height = 0;

		// Textures partagées entre passes
		uint32_t m_OutputTexture = 0; // Résultat du ComputePass (RGBA32F)
		uint32_t m_AccumTexture = 0; // Accumulation (RGBA32F)

		// Quad plein écran partagé (utilisé par FXAAPass)
		uint32_t m_QuadVAO = 0;
		uint32_t m_QuadVBO = 0;

		GPUScene    m_GPUScene;
		ComputePass m_ComputePass;
		FXAAPass    m_FXAAPass;

		// Accumulation
		int      m_FrameIndex = 0;

		// Compteur dédié au mode TAA : incrémente à CHAQUE frame, quel que
		// soit le RenderMode (contrairement à m_FrameIndex, qui reste à 0 en
		// Realtime) — c'est ce qui permet au TAA de lisser même hors accumulation.
		int      m_TAAFrameIndex = 0;
	};

}
