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

	// --------------------------------------------------------------- Groom
	// Génération procédurale de "hair cards" (rubans en triangles, pas de
	// vraies mèches courbes rendues individuellement) à partir de la surface
	// du MeshComponent de la MÊME entité — une entité avec Groom mais sans
	// Mesh ne produit rien. Voir Renderer::uploadSceneData / GroomGenerator
	// pour le pipeline de rendu dédié (les triangles générés sont injectés
	// directement dans le buffer GPU, pas de fichier .obj intermédiaire).
	struct GroomComponent
	{
		int      strandCount = 200;  // Nombre de mèches
		int      segments = 3;       // Segments par mèche (souplesse de la courbe)
		float    length = 0.3f;      // Longueur d'une mèche
		float    thickness = 0.01f;  // Largeur du ruban à la racine
		float    gravity = 0.3f;     // Courbure vers le bas le long de la mèche
		float    randomness = 0.3f;  // Variation longueur/direction par mèche
		uint32_t seed = 1;           // Graine du PRNG (reproductible)
		glm::vec4 color = { 0.25f, 0.15f, 0.1f, 1.f };
	};

	// ----------------------------------------------------------------- Light
	// Directional (soleil) et Point pour l'instant — Area viendra plus tard.
	// Pas de champ direction/position dédié : la directionnelle se dérive de
	// la ROTATION de l'entité (même convention que la caméra, voir
	// ViewportPanel::handleCameraInput), le point de la POSITION — donc les
	// gizmos de transform existants fonctionnent dessus sans rien ajouter.
	enum class LightType { Directional, Point };

	struct LightComponent
	{
		LightType type = LightType::Directional;
		glm::vec3 color = { 1.f, 1.f, 1.f };
		float     intensity = 3.f;

		// Directional : rayon angulaire apparent (rad) — soleil réel ~0.5°.
		// Point : rayon physique de la source (unités monde) — donne des
		// ombres douces et évite une intensité infinie au contact.
		float     radius = 0.0087f;
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