#include "depch.h"
#include "Renderer/GPUScene.h"
#include "Project/Components.h"
#include "Project/Assets/AssetManager.h"
#include "Renderer/GroomGenerator.h"
#include "Application/Application.h"

namespace Fufu
{

	namespace
	{
		// Hash non-cryptographique (FNV-1a sur les octets bruts) : sert
		// uniquement à détecter qu'un GroomComponent a changé depuis la
		// dernière frame, pas à garantir l'unicité. GroomComponent est un POD
		// simple (int/float/uint32_t/glm::vec4), donc le hasher par octets est
		// sûr — pas de pointeur ni de conteneur dedans.
		uint64_t hashBytes(const void* data, std::size_t size)
		{
			const unsigned char* bytes = static_cast<const unsigned char*>(data);
			uint64_t h = 1469598103934665603ull;
			for (std::size_t i = 0; i < size; ++i)
			{
				h ^= bytes[i];
				h *= 1099511628211ull;
			}
			return h;
		}

		uint64_t hashGroomParams(const GroomComponent& g)
		{
			return hashBytes(&g, sizeof(GroomComponent));
		}
	}

	void GPUScene::init()
	{
		glGenBuffers(1, &m_TrianglePositionSSBO);
		glGenBuffers(1, &m_TriangleAttributeSSBO);
		glGenBuffers(1, &m_MaterialSSBO);
		glGenBuffers(1, &m_BLASNodeSSBO);
		glGenBuffers(1, &m_InstanceSSBO);
		glGenBuffers(1, &m_TLASNodeSSBO);
		glGenBuffers(1, &m_LightSSBO);
	}

	void GPUScene::shutdown()
	{
		glDeleteBuffers(1, &m_TrianglePositionSSBO);
		glDeleteBuffers(1, &m_TriangleAttributeSSBO);
		glDeleteBuffers(1, &m_MaterialSSBO);
		glDeleteBuffers(1, &m_BLASNodeSSBO);
		glDeleteBuffers(1, &m_InstanceSSBO);
		glDeleteBuffers(1, &m_TLASNodeSSBO);
		glDeleteBuffers(1, &m_LightSSBO);
	}

	void GPUScene::upload(Scene& scene)
	{
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

		// ------------------------------------------------------------
		// Étape 1 : quelles BLAS (mesh+LOD classique, ou groom+entité) sont
		// nécessaires cette frame, et sont-elles déjà à jour dans le cache
		// persistant (m_BLASCache) ? On ne reconstruit RIEN ici, on détecte
		// juste si un re-upload de la géométrie GPU est nécessaire.
		// ------------------------------------------------------------

		struct NeededMesh { std::shared_ptr<MeshAsset> asset; int lod; };
		std::unordered_map<std::string, NeededMesh> neededMeshes;

		struct NeededGroom { std::shared_ptr<MeshAsset> surfaceAsset; const GroomComponent* groom; uint64_t paramHash; };
		std::unordered_map<std::string, NeededGroom> neededGrooms;

		bool geometryChanged = false;

		auto isCacheEntryStale = [&](const std::string& key, uint64_t sourceVersion)
		{
			auto it = m_BLASCache.find(key);
			return it == m_BLASCache.end() || it->second.sourceVersion != sourceVersion;
		};

		scene.each<TransformComponent, MeshComponent, MaterialComponent>(
			[&](entt::entity /*e*/, TransformComponent& transform, MeshComponent& meshComp, MaterialComponent& /*matComp*/)
		{
			auto asset = am.getMesh(meshComp.meshPath);
			if (!asset || asset->getSubMeshCount() == 0) return;

			float distance = glm::length(transform.position - cameraPos);
			int lod = selectLOD(distance);
			std::string key = meshComp.meshPath + "#lod" + std::to_string(lod);

			neededMeshes.emplace(key, NeededMesh{ asset, lod });
			if (isCacheEntryStale(key, asset->getBLASVersion()))
				geometryChanged = true;
		});

		scene.each<TransformComponent, MeshComponent, GroomComponent>(
			[&](entt::entity e, TransformComponent& /*transform*/, MeshComponent& meshComp, GroomComponent& groom)
		{
			auto surfaceAsset = am.getMesh(meshComp.meshPath);
			if (!surfaceAsset) return;

			std::string key = "groom#" + std::to_string(static_cast<uint32_t>(e));
			uint64_t paramHash = hashGroomParams(groom);

			neededGrooms.emplace(key, NeededGroom{ surfaceAsset, &groom, paramHash });
			if (isCacheEntryStale(key, paramHash))
				geometryChanged = true;
		});

		// Un mesh qui n'est plus référencé par personne (entité supprimée,
		// LOD changé, groom retiré...) doit aussi déclencher un rebuild : sinon
		// le buffer garderait de la géométrie morte, et les offsets cachés ne
		// correspondraient plus à ce que la scène a réellement besoin.
		if (!geometryChanged && neededMeshes.size() + neededGrooms.size() != m_BLASCache.size())
			geometryChanged = true;

		// ------------------------------------------------------------
		// Étape 2 : re-concaténer les buffers GPU de géométrie SEULEMENT si
		// nécessaire — c'est le cœur de l'optimisation : bouger une entité ne
		// passe jamais par ici.
		// ------------------------------------------------------------

		if (geometryChanged)
		{
			m_TrianglePositions.clear();
			m_TriangleAttributes.clear();
			m_BLASNodes.clear();
			std::unordered_map<std::string, BLASRef> freshCache;

			auto append = [&](const std::string& key, const std::vector<GPUTrianglePosition>& positions,
				const std::vector<GPUTriangleAttribute>& attributes,
				const std::vector<GPUBVHNode>& nodes, uint64_t sourceVersion)
			{
				if (positions.empty()) return;

				BLASRef ref{
					static_cast<int>(m_BLASNodes.size()),
					static_cast<int>(m_TrianglePositions.size()),
					sourceVersion
				};
				m_BLASNodes.insert(m_BLASNodes.end(), nodes.begin(), nodes.end());
				m_TrianglePositions.insert(m_TrianglePositions.end(), positions.begin(), positions.end());
				m_TriangleAttributes.insert(m_TriangleAttributes.end(), attributes.begin(), attributes.end());
				freshCache.emplace(key, ref);
			};

			for (auto& [key, needed] : neededMeshes)
			{
				const MeshBLAS& blas = needed.asset->getOrBuildBLAS(needed.lod);
				append(key, blas.positions, blas.attributes, blas.nodes, needed.asset->getBLASVersion());
			}

			for (auto& [key, needed] : neededGrooms)
			{
				// Géométrie procédurale propre à CETTE entité (seed, courbure...) :
				// régénérée à chaque fois que ses paramètres changent (paramHash),
				// jamais partagée entre grooms — mais reste en espace local, donc
				// une entité groom peut quand même bouger sans régénération.
				std::vector<GPUTriangle> groomTris;
				GroomGenerator::generate(*needed.surfaceAsset, *needed.groom, groomTris);

				std::vector<GPUBVHNode> groomNodes;
				std::vector<GPUTrianglePosition> groomPositions;
				std::vector<GPUTriangleAttribute> groomAttributes;
				if (!groomTris.empty())
				{
					groomNodes = BVHBuilder::build(groomTris);
					splitTriangleBuffers(groomTris, groomPositions, groomAttributes);
				}
				append(key, groomPositions, groomAttributes, groomNodes, needed.paramHash);
			}

			m_BLASCache = std::move(freshCache);

			// STATIC_DRAW : ces buffers ne changent que lorsque la géométrie
			// référencée par la scène change réellement, jamais quand un objet
			// se contente de bouger.
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TrianglePositionSSBO);
			glBufferData(GL_SHADER_STORAGE_BUFFER,
				m_TrianglePositions.size() * sizeof(GPUTrianglePosition),
				m_TrianglePositions.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TriangleAttributeSSBO);
			glBufferData(GL_SHADER_STORAGE_BUFFER,
				m_TriangleAttributes.size() * sizeof(GPUTriangleAttribute),
				m_TriangleAttributes.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BLASNodeSSBO);
			glBufferData(GL_SHADER_STORAGE_BUFFER,
				m_BLASNodes.size() * sizeof(GPUBVHNode),
				m_BLASNodes.data(), GL_STATIC_DRAW);
		}

		// ------------------------------------------------------------
		// Étape 3 : instances/matériaux/lumières/TLAS — TOUJOURS reconstruits,
		// mais leur coût est proportionnel au nombre d'entités/lumières, pas
		// au nombre de triangles (donc négligeable même sur un mesh à 50k+
		// triangles). Utilise les offsets de m_BLASCache, qu'ils viennent
		// d'être recalculés à l'étape 2 ou qu'ils datent d'un appel précédent.
		// ------------------------------------------------------------

		m_Materials.clear();
		m_Instances.clear();

		scene.each<TransformComponent, MeshComponent, MaterialComponent>(
			[&](entt::entity /*e*/, TransformComponent& transform, MeshComponent& meshComp, MaterialComponent& matComp)
		{
			auto assetIt = neededMeshes.find(meshComp.meshPath + "#lod" +
				std::to_string(selectLOD(glm::length(transform.position - cameraPos))));
			if (assetIt == neededMeshes.end()) return;

			auto blasIt = m_BLASCache.find(assetIt->first);
			if (blasIt == m_BLASCache.end()) return; // mesh vide (aucun triangle)

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
			inst.blasNodeOffset = blasIt->second.nodeOffset;
			inst.blasTriOffset = blasIt->second.triOffset;
			m_Instances.push_back(inst);
		}
		);

		scene.each<TransformComponent, MeshComponent, GroomComponent>(
			[&](entt::entity e, TransformComponent& transform, MeshComponent& /*meshComp*/, GroomComponent& groom)
		{
			std::string key = "groom#" + std::to_string(static_cast<uint32_t>(e));
			auto blasIt = m_BLASCache.find(key);
			if (blasIt == m_BLASCache.end()) return; // groom vide (0 mèche)

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
			inst.blasNodeOffset = blasIt->second.nodeOffset;
			inst.blasTriOffset = blasIt->second.triOffset;
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
		// de la boîte locale de la racine de son BLAS. Dépend des transforms
		// courantes : reconstruit à chaque upload(), mais son coût est
		// O(instances log instances), pas O(triangles).
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

		// Upload DYNAMIC_DRAW : ces buffers changent potentiellement à chaque
		// frame (transform, matériau, lumière édités), mais leur taille suit le
		// nombre d'entités/lumières, pas le nombre de triangles.
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Materials.size() * sizeof(GPUMaterial),
			m_Materials.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_InstanceSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Instances.size() * sizeof(GPUInstance),
			m_Instances.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TLASNodeSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_TLASNodes.size() * sizeof(GPUBVHNode),
			m_TLASNodes.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_LightSSBO);
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_Lights.size() * sizeof(GPULight),
			m_Lights.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		FUFU_TRACE("Scene uploaded: {} instances, {} unique BLAS triangles, {} materials (geometry {})",
			m_Instances.size(), m_TrianglePositions.size(), m_Materials.size(),
			geometryChanged ? "rebuilt" : "reused");
	}

	void GPUScene::bind() const
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_TrianglePositionSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_MaterialSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_BLASNodeSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_InstanceSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, m_TLASNodeSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, m_LightSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, m_TriangleAttributeSSBO);
	}

}
