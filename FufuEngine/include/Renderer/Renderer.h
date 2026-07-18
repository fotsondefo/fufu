#pragma once

#include "RenderSettings.h"
#include "GPUBuffers.h"
#include "GPUScene.h"
#include "Skybox.h"
#include "Passes/ComputePass.h"
#include "Passes/FXAAPass.h"
#include "Passes/GBufferPass.h"
#include "Passes/ForwardPass.h"
#include "Passes/DeferredPass.h"
#include "Project/Scene/Scene.h"
#include "RHI/RHIContext.h"
#include <filesystem>

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

		// Accès au GPUScene pour les outils de visualisation (FufuLab)
		GPUScene&       getGPUScene()       { return m_GPUScene; }
		const GPUScene& getGPUScene() const { return m_GPUScene; }

		// Remet l'accumulation � z�ro (ex: cam�ra boug�e)
		void resetAccumulation();

		int getAccumulatedFrames() const { return m_FrameIndex; }

		// Écrit l'image actuellement affichée (voir getOutputTextureID) dans un
		// PNG 8 bits. L'image de sortie est déjà tone-mappée/gamma-corrigée par
		// le compute shader, donc pas de retraitement nécessaire ici — juste
		// une lecture GPU->CPU et une conversion float[0,1] -> uint8.
		bool exportImage(const std::filesystem::path& path) const;

		// Texture à afficher : GL handle (uint32_t) extrait du RHITexture.
		uint32_t getOutputTextureID() const;

	private:
		// Init OpenGL
		void createTextures();
		void createQuad();

		bool sceneNeedsUpdate(Scene& scene);

		// Efface la texture de sortie (scène sans caméra primaire) : sinon
		// l'image de la scène précédente reste affichée telle quelle.
		void clearOutput();

	private:
		RHI::Ref<RHI::RHIContext> m_RHIContext;

		RenderSettings m_Settings;

		int m_Width = 0;
		int m_Height = 0;

		// Textures partagées entre passes
		uint32_t m_OutputTexture = 0; // Résultat du ComputePass (RGBA32F)
		uint32_t m_AccumTexture = 0; // Accumulation (RGBA32F)

		// Quad plein écran partagé (utilisé par FXAAPass)
		uint32_t m_QuadVAO = 0;
		uint32_t m_QuadVBO = 0;

		GPUScene     m_GPUScene;
		Skybox       m_Skybox;
		ComputePass  m_ComputePass;
		FXAAPass     m_FXAAPass;
		GBufferPass  m_GBufferPass;
		ForwardPass  m_ForwardPass;
		DeferredPass m_DeferredPass;

		// Accumulation
		int      m_FrameIndex = 0;

		// Compteur dédié au mode TAA : incrémente à CHAQUE frame, quel que
		// soit le RenderMode (contrairement à m_FrameIndex, qui reste à 0 en
		// Realtime) — c'est ce qui permet au TAA de lisser même hors accumulation.
		int      m_TAAFrameIndex = 0;

		// Cache pour sceneNeedsUpdate : re-upload seulement si la scène a
		// réellement changé (Scene::getVersion(), voir Scene::markDirty) OU si
		// la scène active a changé d'identité (switch de scène) — comparer la
		// seule version ne suffirait pas dans ce second cas si la nouvelle
		// scène active a, par coïncidence, la même version numérique en cache.
		Scene*   m_LastScene = nullptr;
		uint32_t m_LastSceneVersion = 0;
	};

}
