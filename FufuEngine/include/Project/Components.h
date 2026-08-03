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
		glm::vec3 rotation = { 0.f, 0.f, 0.f }; // Euler angles in radians
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
	// Procedural generation of "hair cards" (triangle strips, not real curved
	// strands rendered individually) from the surface of the MeshComponent of
	// the SAME entity — an entity with Groom but no Mesh produces nothing.
	// See Renderer::uploadSceneData / GroomGenerator for the dedicated render
	// pipeline (generated triangles are injected directly into the GPU buffer,
	// no intermediate .obj file).
	struct GroomComponent
	{
		int      strandCount = 200;  // Number of strands
		int      segments = 3;       // Segments per strand (curve flexibility)
		float    length = 0.3f;      // Strand length
		float    thickness = 0.01f;  // Strip width at the root
		float    gravity = 0.3f;     // Downward curvature along the strand
		float    randomness = 0.3f;  // Per-strand length/direction variation
		uint32_t seed = 1;           // PRNG seed (reproducible)
		glm::vec4 color = { 0.25f, 0.15f, 0.1f, 1.f };
	};

	// ----------------------------------------------------------------- Light
	// Directional (sun) and Point for now — Area will come later.
	// No dedicated direction/position field: the directional is derived from
	// the entity ROTATION (same convention as the camera, see
	// ViewportPanel::handleCameraInput), the point from the POSITION — so
	// existing transform gizmos work on them without adding anything.
	enum class LightType { Directional, Point };

	// Physical light unit. GPUScene converts to a common internal scale before
	// uploading to the GPU so shaders don't need to know the chosen unit.
	//   Arbitrary : raw multiplier (default, backward-compatible)
	//   Lux       : lm/m² — illuminance at a surface (directional: ~100 000 lx = sun)
	//   Lumen     : total luminous flux; converted to candela via I = lm / (4π)
	//   Candela   : luminous intensity (lm/sr) — direct use
	enum class LightUnit { Arbitrary, Lux, Lumen, Candela };

	struct LightComponent
	{
		LightType type      = LightType::Directional;
		LightUnit unit      = LightUnit::Arbitrary;
		glm::vec3 color     = { 1.f, 1.f, 1.f };
		float     intensity = 3.f;

		// Directional: apparent angular radius (rad) — real sun ~0.5°.
		// Point: physical radius of the source (world units) — produces soft
		// shadows and avoids infinite intensity at contact.
		float     radius = 0.0087f;
	};

	// ------------------------------------------------------------- Material
	struct MaterialComponent
	{
		glm::vec4 albedo    = { 1.f, 1.f, 1.f, 1.f };
		float     metallic  = 0.f;
		float     roughness = 1.f;
		float     emissive  = 0.f;
		float     ior       = 1.5f; // dielectric reflectance: F0 = ((ior-1)/(ior+1))^2

		uint64_t  albedoTexID = 0; // UUID texture (0 = none)
		uint64_t  normalTexID = 0; // tangent-space normal map
		uint64_t  ormTexID    = 0; // ORM packed: R=AO, G=Roughness, B=Metallic
	};

	// --------------------------------------------------------- Per-submesh materials
	// Optional override of MaterialComponent: one slot per SubMesh in the mesh.
	// When present, GPUScene creates N consecutive GPUMaterials (one per slot)
	// and the vertex shader adds the per-triangle submesh index to the base
	// materialIndex — so each submesh gets its own shading independently.
	// Slots shorter than the actual submesh count fall back to MaterialComponent.
	struct SubMeshMaterialsComponent
	{
		std::vector<MaterialComponent> slots;
	};

	// ---------------------------------------------------------- Prefab
	// Marks the root of an entity instantiated from a prefab. For now this is
	// a simple provenance link (frozen snapshot, no synchronization):
	// see PrefabSerializer.
	struct PrefabInstanceComponent
	{
		std::string prefabPath;

		PrefabInstanceComponent() = default;
		explicit PrefabInstanceComponent(const std::string& prefabPath) : prefabPath(prefabPath) {}
	};

	// ---------------------------------------------------------- Animator
	// Drives skeletal animation on a MeshComponent that has bones.
	// currentBoneMatrices is recomputed every frame by Renderer::advanceAnimations().
	struct AnimatorComponent
	{
		int   clipIndex = 0;
		float time      = 0.f;   // current playback time in seconds
		bool  playing   = true;
		bool  loop      = true;
		float speed     = 1.f;

		// Computed per frame — read by GPUScene::updateSkinning()
		std::vector<glm::mat4> currentBoneMatrices;
	};

	// ----------------------------------------------------------------- Volume
	// Heterogeneous participative medium: axis-aligned box in world space.
	// Position/scale come from the entity's TransformComponent (scale = full extent).
	// VolumePass generates a 3D density texture from the noise parameters.
	enum class VolumeNoiseType { None, Value, FBM };

	struct VolumeComponent
	{
		// Appearance
		glm::vec3 albedo           = { 1.f,  1.f,  1.f  };
		glm::vec3 emission         = { 1.f, 0.6f, 0.1f  };
		float     density          = 1.f;
		float     scattering       = 0.5f;
		float     absorption       = 0.1f;
		float     emissionStrength = 0.f;
		float     anisotropy       = 0.f;   // Henyey-Greenstein g ∈ [-1, 1]

		// Density field (procedural — no asset loading yet)
		VolumeNoiseType noiseType    = VolumeNoiseType::None;
		float           noiseScale   = 4.f;
		int             noiseOctaves = 4;
		float           noiseLacunarity = 2.f;
		float           noiseGain    = 0.5f;

		int marchSteps = 64;
	};

	// ------------------------------------------------------------- GaussianSplat
	// 3D Gaussian Splatting: loads a .ply capture file and renders it as a cloud
	// of alpha-blended oriented Gaussian ellipsoids. TransformComponent applies a
	// world transform (position / rotation / scale) to the whole splat cloud.
	struct GaussianSplatComponent
	{
		std::string path;       // Path to .ply file
		uint64_t    assetID = 0;
		float       opacity = 1.f;   // Global opacity multiplier
		int         shDegree = 3;    // Spherical harmonics degree (0=DC only, 1-3)
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