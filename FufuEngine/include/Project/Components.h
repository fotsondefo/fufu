#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Fufu 
{

	// ------------------------------------------------------------------ Tag
	struct TagComponent
	{
		std::string tag;

		TagComponent() = default;
		explicit TagComponent(const std::string& tag) : tag(tag) {}
	};

	// ------------------------------------------------------------ Transform
	struct TransformComponent
	{
		glm::vec3 position = { 0.f, 0.f, 0.f };
		glm::vec3 rotation = { 0.f, 0.f, 0.f }; // Euler en radians
		glm::vec3 scale = { 1.f, 1.f, 1.f };

		glm::mat4 getTransform() const
		{
			return glm::translate(glm::mat4(1.f), position)
				* glm::toMat4(glm::quat(rotation))
				* glm::scale(glm::mat4(1.f), scale);
		}
	};

	// ------------------------------------------------------- Hierarchy
	struct ParentComponent
	{
		entt::entity parent = entt::null;
	};

	struct ChildrenComponent
	{
		std::vector<entt::entity> children;

		void addChild(entt::entity e)
		{
			children.push_back(e);
		}

		void removeChild(entt::entity e)
		{
			children.erase(std::remove(children.begin(), children.end(), e), children.end());
		}
	};

	// ----------------------------------------------------------------- Mesh
	struct MeshComponent
	{
		std::string meshPath;   // Source file path (Assimp)
		uint64_t    meshID = 0; // UUID resolved by the AssetManager
	};

	// ------------------------------------------------------------- Material
	struct MaterialComponent
	{
		glm::vec4 albedo = { 1.f, 1.f, 1.f, 1.f };
		float     metallic = 0.f;
		float     roughness = 1.f;
		float     emissive = 0.f;

		uint64_t  albedoTexID = 0; // UUID texture (0 = no texture)
		uint64_t  normalTexID = 0;
	};

	// ---------------------------------------------------------- Prefab
	// Marque la racine d'une entité instanciée depuis un prefab. Pour
	// l'instant c'est un simple lien de provenance (snapshot figé, pas de
	// synchronisation) : voir PrefabSerializer.
	struct PrefabInstanceComponent
	{
		std::string prefabPath;

		PrefabInstanceComponent() = default;
		explicit PrefabInstanceComponent(const std::string& prefabPath) : prefabPath(prefabPath) {}
	};

	// --------------------------------------------------------------- Camera
	enum class CameraProjection { Perspective, Orthographic };

	struct CameraComponent
	{
		CameraProjection projection = CameraProjection::Perspective;
		float            fov = glm::radians(45.f);
		float            nearPlane = 0.1f;
		float            farPlane = 1000.f;
		float            orthoSize = 10.f;
		bool             primary = false; // Active camera (the one use to navigate through the scene)

		glm::mat4 getProjectionMatrix(float aspectRatio) const
		{
			if (projection == CameraProjection::Perspective)
				return glm::perspective(fov, aspectRatio, nearPlane, farPlane);

			float half = orthoSize * 0.5f;
			return glm::ortho(-half * aspectRatio, half * aspectRatio,
				-half, half,
				nearPlane, farPlane);
		}
	};

}