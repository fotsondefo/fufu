#include "depch.h"
#include "Renderer/GPUScene.h"
#include "Project/Components.h"
#include "Project/Assets/AssetManager.h"
#include "Renderer/GroomGenerator.h"
#include "Application/Application.h"

namespace Fufu
{

	void GPUScene::init()
	{
		glGenBuffers(1, &m_TriangleSSBO);
		glGenBuffers(1, &m_MaterialSSBO);
		glGenBuffers(1, &m_BLASNodeSSBO);
		glGenBuffers(1, &m_InstanceSSBO);
		glGenBuffers(1, &m_TLASNodeSSBO);
		glGenBuffers(1, &m_LightSSBO);
	}

	void GPUScene::shutdown()
	{
		glDeleteBuffers(1, &m_TriangleSSBO);
		glDeleteBuffers(1, &m_MaterialSSBO);
		glDeleteBuffers(1, &m_BLASNodeSSBO);
		glDeleteBuffers(1, &m_InstanceSSBO);
		glDeleteBuffers(1, &m_TLASNodeSSBO);
		glDeleteBuffers(1, &m_LightSSBO);
	}

	void GPUScene::upload(Scene& scene)
	{
		m_Triangles.clear();
		m_Materials.clear();
		m_BLASNodes.clear();
		m_Instances.clear();

		auto& am = Fufu::Application::get().getProjectManager().getCurrentProject().getAssetManager();

		// Caméra active : sert à choisir le LOD par distance (voir selectLOD).
		glm::vec3 cameraPos(0.f);
		if (Entity cam = scene.getPrimaryCamera())
			cameraPos = cam.getComponent<TransformComponent>().position;

		// Seuils de distance volontairement simples (pas de hystérésis ni de
		// notion de taille à l'écran) : LOD0 en dessous de 15 unités, LOD1
		// jusqu'à 40, LOD2 au-delà. Un "pop" est possible en traversant un
		// seuil — acceptable pour cette première itération.
		auto selectLOD = [](float distance) -> int
		{
			if (distance > 40.f) return 2;
			if (distance > 15.f) return 1;
			return 0;
		};

		// Cache BLAS par (chemin de mesh, niveau de LOD) : construit une seule
		// fois par combinaison unique, même si N entités la référencent —
		// c'est l'instancing. Reconstruit à chaque frame comme le reste de
		// l'upload (cohérent avec le sculpt/extrude qui mutent LOD0 en
		// mémoire ; voir MeshAsset::invalidateLODs pour les LOD générés).
		struct BLASRef { int nodeOffset; int triOffset; };
		std::unordered_map<std::string, BLASRef> blasCache;

		auto getOrBuildBLAS = [&](const std::string& path, int lod) -> const BLASRef*
		{
			std::string key = path + "#lod" + std::to_string(lod);
			auto found = blasCache.find(key);
			if (found != blasCache.end())
				return &found->second;

			auto meshAsset = am.getMesh(path);
			if (!meshAsset || meshAsset->getSubMeshCount() == 0)
				return nullptr;

			std::vector<GPUTriangle> localTris;
			for (const auto& sub : meshAsset->getLODSubMeshes(lod))
			{
				for (std::size_t i = 0; i + 2 < sub.indices.size(); i += 3)
				{
					const Vertex& a = sub.vertices[sub.indices[i]];
					const Vertex& b = sub.vertices[sub.indices[i + 1]];
					const Vertex& c = sub.vertices[sub.indices[i + 2]];

					GPUTriangle tri{};
					tri.v0 = glm::vec4(a.position, 0.f);
					tri.v1 = glm::vec4(b.position, 0.f);
					tri.v2 = glm::vec4(c.position, 0.f);
					tri.n0 = glm::vec4(a.normal, 0.f);
					tri.n1 = glm::vec4(b.normal, 0.f);
					tri.n2 = glm::vec4(c.normal, 0.f);
					tri.uv0 = a.uv;
					tri.uv1 = b.uv;
					tri.uv2 = c.uv;
					tri.materialIndex = 0; // le matériau vient de l'instance, pas du BLAS partagé
					localTris.push_back(tri);
				}
			}

			if (localTris.empty())
				return nullptr;

			std::vector<GPUBVHNode> localNodes = BVHBuilder::build(localTris);

			BLASRef ref{ static_cast<int>(m_BLASNodes.size()), static_cast<int>(m_Triangles.size()) };
			m_BLASNodes.insert(m_BLASNodes.end(), localNodes.begin(), localNodes.end());
			m_Triangles.insert(m_Triangles.end(), localTris.begin(), localTris.end());

			return &blasCache.emplace(std::move(key), ref).first->second;
		};

		// Meshes "classiques" : le même BLAS (mesh, LOD) peut être partagé par plusieurs instances.
		scene.each<TransformComponent, MeshComponent, MaterialComponent>(
			[&](entt::entity /*e*/,
				TransformComponent& transform,
				MeshComponent& meshComp,
				MaterialComponent& matComp)
		{
			float distance = glm::length(transform.position - cameraPos);
			const BLASRef* blas = getOrBuildBLAS(meshComp.meshPath, selectLOD(distance));
			if (!blas) return;

			int matIdx = static_cast<int>(m_Materials.size());
			GPUMaterial gpuMat;
			gpuMat.albedo = matComp.albedo;
			gpuMat.metallic = matComp.metallic;
			gpuMat.roughness = matComp.roughness;
			gpuMat.emissive = matComp.emissive;
			gpuMat.ior = 1.5f;
			gpuMat.albedoTexIdx = -1;
			m_Materials.push_back(gpuMat);

			GPUInstance inst{};
			inst.transform = transform.getTransform();
			inst.invTransform = glm::inverse(inst.transform);
			inst.materialIndex = matIdx;
			inst.blasNodeOffset = blas->nodeOffset;
			inst.blasTriOffset = blas->triOffset;
			m_Instances.push_back(inst);
		}
		);

		// Groom : géométrie procédurale propre à CHAQUE entité (seed, courbure...),
		// donc pas de partage de BLAS entre groom — mais même chemin d'instanciation
		// que les meshes classiques (une instance, un BLAS dédié).
		scene.each<TransformComponent, MeshComponent, GroomComponent>(
			[&](entt::entity /*e*/,
				TransformComponent& transform,
				MeshComponent& meshComp,
				GroomComponent& groom)
		{
			auto meshAsset = am.getMesh(meshComp.meshPath);
			if (!meshAsset) return;

			std::vector<GPUTriangle> groomTris;
			GroomGenerator::generate(*meshAsset, groom, groomTris);
			if (groomTris.empty()) return;

			std::vector<GPUBVHNode> groomNodes = BVHBuilder::build(groomTris);

			int nodeOffset = static_cast<int>(m_BLASNodes.size());
			int triOffset = static_cast<int>(m_Triangles.size());
			m_BLASNodes.insert(m_BLASNodes.end(), groomNodes.begin(), groomNodes.end());
			m_Triangles.insert(m_Triangles.end(), groomTris.begin(), groomTris.end());

			int matIdx = static_cast<int>(m_Materials.size());
			GPUMaterial gpuMat;
			gpuMat.albedo = groom.color;
			gpuMat.metallic = 0.f;
			gpuMat.roughness = 0.9f;
			gpuMat.emissive = 0.f;
			gpuMat.ior = 1.4f;
			gpuMat.albedoTexIdx = -1;
			m_Materials.push_back(gpuMat);

			GPUInstance inst{};
			inst.transform = transform.getTransform();
			inst.invTransform = glm::inverse(inst.transform);
			inst.materialIndex = matIdx;
			inst.blasNodeOffset = nodeOffset;
			inst.blasTriOffset = triOffset;
			m_Instances.push_back(inst);
		}
		);

		// Lumières : la Directional dérive sa direction de la ROTATION de
		// l'entité (même convention que la caméra), la Point de sa POSITION —
		// pas de champ dédié, les gizmos de transform existants suffisent.
		m_Lights.clear();
		scene.each<TransformComponent, LightComponent>(
			[&](entt::entity /*e*/, TransformComponent& transform, LightComponent& light)
		{
			GPULight gpuLight{};
			gpuLight.color = glm::vec4(light.color, light.intensity);
			gpuLight.radius = light.radius;

			if (light.type == LightType::Directional)
			{
				glm::quat rotation = glm::quat(transform.rotation);
				glm::vec3 forward = glm::normalize(rotation * glm::vec3(0.f, 0.f, -1.f));
				gpuLight.positionOrDirection = glm::vec4(-forward, 0.f); // de la surface VERS la lumière
				gpuLight.type = 0;
			}
			else
			{
				gpuLight.positionOrDirection = glm::vec4(transform.position, 0.f);
				gpuLight.type = 1;
			}

			m_Lights.push_back(gpuLight);
		}
		);

		// TLAS : une boîte englobante MONDE par instance, dérivée des 8 coins
		// de la boîte locale de la racine de son BLAS.
		std::vector<glm::vec3> instBoundsMin(m_Instances.size());
		std::vector<glm::vec3> instBoundsMax(m_Instances.size());

		for (std::size_t i = 0; i < m_Instances.size(); ++i)
		{
			const GPUInstance& inst = m_Instances[i];
			const GPUBVHNode& root = m_BLASNodes[static_cast<std::size_t>(inst.blasNodeOffset)];

			glm::vec3 lmin = glm::vec3(root.boundsMin);
			glm::vec3 lmax = glm::vec3(root.boundsMax);

			glm::vec3 worldMin(1e30f), worldMax(-1e30f);
			for (int c = 0; c < 8; ++c)
			{
				glm::vec3 corner(
					(c & 1) ? lmax.x : lmin.x,
					(c & 2) ? lmax.y : lmin.y,
					(c & 4) ? lmax.z : lmin.z);
				glm::vec3 worldCorner = glm::vec3(inst.transform * glm::vec4(corner, 1.f));
				worldMin = glm::min(worldMin, worldCorner);
				worldMax = glm::max(worldMax, worldCorner);
			}

			instBoundsMin[i] = worldMin;
			instBoundsMax[i] = worldMax;
		}

		std::vector<int> order;
		m_TLASNodes = BVHBuilder::buildFromBounds(instBoundsMin, instBoundsMax, order);

		std::vector<GPUInstance> reorderedInstances(m_Instances.size());
		for (std::size_t i = 0; i < order.size(); ++i)
			reorderedInstances[i] = m_Instances[static_cast<std::size_t>(order[i])];
		m_Instances = std::move(reorderedInstances);

		// Upload triangles (BLAS, espace local, concaténés par mesh unique)
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TriangleSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Triangles.size() * sizeof(GPUTriangle),
			m_Triangles.data(), GL_DYNAMIC_DRAW);

		// Upload materials
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Materials.size() * sizeof(GPUMaterial),
			m_Materials.data(), GL_DYNAMIC_DRAW);

		// Upload nœuds BLAS
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BLASNodeSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_BLASNodes.size() * sizeof(GPUBVHNode),
			m_BLASNodes.data(), GL_DYNAMIC_DRAW);

		// Upload instances
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_InstanceSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Instances.size() * sizeof(GPUInstance),
			m_Instances.data(), GL_DYNAMIC_DRAW);

		// Upload TLAS
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TLASNodeSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_TLASNodes.size() * sizeof(GPUBVHNode),
			m_TLASNodes.data(), GL_DYNAMIC_DRAW);

		// Upload lumières
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_LightSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Lights.size() * sizeof(GPULight),
			m_Lights.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		FUFU_TRACE("Scene uploaded: {} instances, {} unique BLAS triangles, {} materials",
			m_Instances.size(), m_Triangles.size(), m_Materials.size());
	}

	void GPUScene::bind() const
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_TriangleSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_MaterialSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_BLASNodeSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_InstanceSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, m_TLASNodeSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, m_LightSSBO);
	}

}
