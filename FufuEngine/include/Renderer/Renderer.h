#pragma once

#include "RenderSettings.h"
#include "GPUBuffers.h"
#include "GPUScene.h"
#include "Skybox.h"
#include "Passes/ComputePass.h"
#include "Passes/FXAAPass.h"
#include "Passes/ToneMappingPass.h"
#include "Passes/BloomPass.h"
#include "Passes/ShadowPass.h"
#include "Passes/SSAOPass.h"
#include "Passes/DofPass.h"
#include "Passes/VolumetricPass.h"
#include "Passes/VolumePass.h"
#include "Passes/GaussianSplatPass.h"
#include "Passes/GBufferPass.h"
#include "Passes/ForwardPass.h"
#include "Passes/DeferredPass.h"
#include "Project/Scene/Scene.h"
#include "RHI/RHIContext.h"
#include "Renderer/FileWatcher.h"
#include <filesystem>

namespace Fufu
{

	// Orchestrator: owns the textures shared between passes and the accumulation
	// state, builds the GPU camera/frame from the Scene/ECS (the only part that
	// stays here rather than in a pass, because it depends on components), then
	// delegates the bulk of the work to GPUScene (geometry packing) and the
	// passes (ComputePass, FXAAPass).
	class Renderer
	{
	public:
		Renderer() = default;
		~Renderer() = default;

		void init(int width, int height);
		void shutdown();

		// Called every frame from Application::run()
		void renderScene(Scene& scene);

		// Viewport resize
		void resize(int width, int height);

		RenderSettings& getSettings() { return m_Settings; }

		// Access to GPUScene for visualization tools (FufuLab)
		GPUScene&       getGPUScene()       { return m_GPUScene; }
		const GPUScene& getGPUScene() const { return m_GPUScene; }

		// Resets accumulation (e.g. camera moved)
		void resetAccumulation();

		// Shader hot-reload: registers each pass's source files with the FileWatcher.
		// The watcher is ticked every frame inside renderScene().
		// Call once after init() — or again after resize() if needed.
		void initShaderWatcher();

		int getAccumulatedFrames() const { return m_FrameIndex; }

		// Writes the currently displayed image (see getOutputTextureID) to an
		// 8-bit PNG. The output image is already tone-mapped/gamma-corrected by
		// the compute shader, so no reprocessing is needed here — just a GPU->CPU
		// read and a float[0,1] -> uint8 conversion.
		bool exportImage(const std::filesystem::path& path) const;

		// Texture to display: GL handle (uint32_t) extracted from RHITexture.
		uint32_t getOutputTextureID() const;

	private:
		// OpenGL init
		void createTextures();
		void createQuad();

		bool sceneNeedsUpdate(Scene& scene);

		// Clears the output texture (scene with no primary camera): otherwise
		// the previous scene's image remains displayed as-is.
		void clearOutput();

	private:
		RHI::Ref<RHI::RHIContext> m_RHIContext;

		RenderSettings m_Settings;

		int m_Width = 0;
		int m_Height = 0;

		// Textures shared between passes
		uint32_t m_OutputTexture = 0; // ComputePass result (RGBA32F)
		uint32_t m_AccumTexture = 0; // Accumulation (RGBA32F)

		// Shared fullscreen quad (used by FXAAPass)
		uint32_t m_QuadVAO = 0;
		uint32_t m_QuadVBO = 0;

		GPUScene          m_GPUScene;
		Skybox            m_Skybox;
		ComputePass       m_ComputePass;
		FXAAPass          m_FXAAPass;
		ToneMappingPass   m_ToneMappingPass;
		BloomPass         m_BloomPass;
		ShadowPass        m_ShadowPass;
		SSAOPass          m_SSAOPass;
		DofPass           m_DofPass;
		VolumetricPass    m_VolumetricPass;
		VolumePass        m_VolumePass;
		GaussianSplatPass m_GaussianSplatPass;
		GBufferPass       m_GBufferPass;
		ForwardPass       m_ForwardPass;
		DeferredPass      m_DeferredPass;

		FileWatcher m_ShaderWatcher;

		// Accumulation
		int      m_FrameIndex = 0;

		// Counter dedicated to TAA mode: increments on EVERY frame regardless
		// of RenderMode (unlike m_FrameIndex, which stays at 0 in Realtime) —
		// this is what lets TAA smooth even outside accumulation mode.
		int      m_TAAFrameIndex = 0;

		// Cache for sceneNeedsUpdate: re-upload only if the scene has actually
		// changed (Scene::getVersion(), see Scene::markDirty) OR if the active
		// scene changed identity (scene switch) — comparing the version alone
		// would not suffice in that second case if the new active scene
		// coincidentally has the same cached numeric version.
		Scene*   m_LastScene = nullptr;
		uint32_t m_LastSceneVersion = 0;
	};

}
