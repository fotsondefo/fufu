#pragma once

#include "RenderSettings.h"
#include "GPUBuffers.h"
#include "BVH.h"
#include "Project/Scene/Scene.h"

namespace Fufu 
{

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

		// R�cup�re la texture finale (pour affichage ImGui ou blit)
		uint32_t getOutputTextureID() const { return m_OutputTexture; }

		RenderSettings& getSettings() { return m_Settings; }

		// Remet l'accumulation � z�ro (ex: cam�ra boug�e)
		void resetAccumulation();

		int getAccumulatedFrames() const { return m_FrameIndex; }

	private:
		// Init OpenGL
		void createTextures();
		void createShaders();
		void createSSBOs();
		void createQuad();

		// Upload scene ? GPU
		void uploadSceneData(Scene& scene);
		bool sceneNeedsUpdate(Scene& scene);

		// Passes de rendu
		void computePass();
		void blitPass();

		// Compilation shader
		uint32_t compileShader(uint32_t type, const std::string& source);
		uint32_t linkProgram(std::initializer_list<uint32_t> shaders);

	private:
		RenderSettings m_Settings;

		int m_Width = 0;
		int m_Height = 0;

		// Textures
		uint32_t m_OutputTexture = 0; // R�sultat final (RGBA32F)
		uint32_t m_AccumTexture = 0; // Accumulation (RGBA32F)

		// Programs OpenGL
		uint32_t m_ComputeProgram = 0;
		uint32_t m_BlitProgram = 0;

		// SSBOs
		uint32_t m_TriangleSSBO = 0;  // BLAS : triangles en espace local, concaténés par mesh unique
		uint32_t m_MaterialSSBO = 0;
		uint32_t m_BLASNodeSSBO = 0;  // BLAS : nœuds BVH, concaténés par mesh unique
		uint32_t m_InstanceSSBO = 0;  // une entrée par instance (transform + réf. BLAS + matériau)
		uint32_t m_TLASNodeSSBO = 0;  // BVH de plus haut niveau, sur les boîtes des instances
		uint32_t m_CameraUBO = 0;
		uint32_t m_FrameDataUBO = 0;

		// Quad de blit
		uint32_t m_QuadVAO = 0;
		uint32_t m_QuadVBO = 0;

		// Accumulation
		int      m_FrameIndex = 0;

		// Version tracking pour �viter les uploads inutiles
		uint32_t m_LastSceneVersion = 0;

		// Cache des donn�es GPU
		std::vector<GPUTriangle> m_Triangles;   // BLAS, espace local, concat�n�s par mesh unique
		std::vector<GPUMaterial> m_Materials;
		std::vector<GPUBVHNode>  m_BLASNodes;   // BLAS, concat�n�s par mesh unique
		std::vector<GPUInstance> m_Instances;
		std::vector<GPUBVHNode>  m_TLASNodes;
	};

}