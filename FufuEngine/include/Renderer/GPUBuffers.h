#pragma once

#pragma once
#include <glm/glm.hpp>
#include <vector>


namespace Fufu
{

	// Maximum number of distinct albedo textures bindable in one frame
	// (texture units 1..kMaxMaterialTextures, unit 0 being reserved for the
	// skybox — see ComputePass). Must match the size of the
	// `u_MaterialTextures[]` array in PathTracer.comp.
	constexpr int kMaxMaterialTextures = 16;

	// CPU-side mirror structures of what is sent to the GPU via SSBO.
	// Strict std430 alignment: each field respects GLSL alignment rules.

	struct alignas(16) GPUSphere
	{
		glm::vec3 center;
		float     radius;
		int       materialIndex;
		float     _pad[3];
	};

	// Intermediate CPU format (NOT a SSBO layout): used only during
	// triangulation + BVHBuilder::build (which reorders this vector in place
	// so each leaf references a contiguous range). Once the BVH is built,
	// splitTriangleBuffers() splits this combined format into the two GPU
	// buffers actually uploaded — see GPUTrianglePosition/GPUTriangleAttribute.
	struct GPUTriangle
	{
		glm::vec4 v0;           // xyz = position, w = unused
		glm::vec4 v1;
		glm::vec4 v2;
		glm::vec4 n0;           // per-vertex normals
		glm::vec4 n1;
		glm::vec4 n2;
		glm::vec2 uv0;
		glm::vec2 uv1;
		glm::vec2 uv2;
		int       materialIndex;
		float     _pad;
		// CPU-only skinning data — carried through BVH reorder so skin weights
		// end up in the same leaf order as blas.positions.
		glm::ivec4 boneIdx0{0,0,0,0}; glm::vec4 boneWgt0{0.f,0.f,0.f,0.f};
		glm::ivec4 boneIdx1{0,0,0,0}; glm::vec4 boneWgt1{0.f,0.f,0.f,0.f};
		glm::ivec4 boneIdx2{0,0,0,0}; glm::vec4 boneWgt2{0.f,0.f,0.f,0.f};
	};

	// Per-triangle skin weights (CPU struct, not a SSBO).
	// Stored in MeshBLAS::skinWeights[] parallel to positions[].
	struct GPUSkinTriangle
	{
		glm::ivec4 idx0; glm::vec4 wgt0;   // v0
		glm::ivec4 idx1; glm::vec4 wgt1;   // v1
		glm::ivec4 idx2; glm::vec4 wgt2;   // v2
	};

	// INTERSECTION buffer: the only buffer read during BVH traversal (inner
	// loop of the path/ray tracer). 48 bytes, std430 alignment already natural
	// (3x vec4) — no normals/UVs/material here, to reduce memory traffic during
	// the hottest part of the compute shader.
	struct alignas(16) GPUTrianglePosition
	{
		glm::vec4 v0; // xyz = position, w unused
		glm::vec4 v1;
		glm::vec4 v2;
	};

	// ATTRIBUTE buffer: read ONCE, after finding the ray's final hit point
	// (see resolveHit() on the shader side) — never during traversal.
	// materialIndex is a remnant: currently always overridden by the instance's
	// material (a BLAS can be shared by multiple instances with different
	// materials), kept for future per-mesh multi-material support.
	struct alignas(16) GPUTriangleAttribute
	{
		glm::vec4 n0;
		glm::vec4 n1;
		glm::vec4 n2;
		glm::vec2 uv0;
		glm::vec2 uv1;
		glm::vec2 uv2;
		int       materialIndex;
		float     _pad;
	};

	// Splits the combined CPU format (post-BVHBuilder::build, so already in
	// leaf order) into the two GPU buffers — same indices on both sides, so
	// resolveHit() can reuse the index found during position traversal without
	// recomputing it.
	inline void splitTriangleBuffers(const std::vector<GPUTriangle>& combined,
		std::vector<GPUTrianglePosition>& outPositions,
		std::vector<GPUTriangleAttribute>& outAttributes)
	{
		outPositions.reserve(outPositions.size() + combined.size());
		outAttributes.reserve(outAttributes.size() + combined.size());

		for (const GPUTriangle& tri : combined)
		{
			GPUTrianglePosition pos{ tri.v0, tri.v1, tri.v2 };
			outPositions.push_back(pos);

			GPUTriangleAttribute attr{};
			attr.n0 = tri.n0;
			attr.n1 = tri.n1;
			attr.n2 = tri.n2;
			attr.uv0 = tri.uv0;
			attr.uv1 = tri.uv1;
			attr.uv2 = tri.uv2;
			attr.materialIndex = tri.materialIndex;
			outAttributes.push_back(attr);
		}
	}

	// An instance = one copy of a BLAS (mesh in local space) with its own
	// transform and its own material. blasNodeOffset/blasTriOffset point into
	// the shared BLASNodeBuffer/TriangleBuffer, which concatenate the BLAS of
	// all unique meshes in the scene.
	struct alignas(16) GPUInstance
	{
		glm::mat4 transform;    // local -> monde
		glm::mat4 invTransform; // world -> local (to transform the ray)
		int materialIndex;
		int blasNodeOffset;
		int blasTriOffset;
		int _pad;
	};

	struct alignas(16) GPUMaterial
	{
<<<<<<< HEAD
		glm::vec4 albedo;         // rgb = color, a = opacity
		float     metallic;
		float     roughness;
		float     emissive;
		float     ior;            // Index of refraction (dielectric F0 = ((ior-1)/(ior+1))^2)
		int       albedoTexIdx;   // -1 = no texture (index into shared texture pool)
		int       normalTexIdx;   // tangent-space normal map (-1 = flat)
		int       ormTexIdx;      // ORM packed: R=AO, G=Roughness, B=Metallic (-1 = use constants)
		float     _pad;
=======
		glm::vec4 albedo;       // rgb = color, a = opacity
		float     metallic;
		float     roughness;
		float     emissive;
		float     ior;          // Index of refraction (glass = 1.5)
		int       albedoTexIdx; // -1 = no texture
		float     _pad[3];
>>>>>>> c984e4df3b1c22d177ab4019fa05d517ae4c3474
	};

	// Directional: positionOrDirection.xyz = direction from a surface TOWARDS
	// the light (normalized), radius = angular radius (rad).
	// Point:       positionOrDirection.xyz = world position, radius = physical
	// radius of the source (world units) — 1/distance² attenuation handled
	// on the shader side, not here.
	struct alignas(16) GPULight
	{
		glm::vec4 positionOrDirection;
		glm::vec4 color;    // rgb = color, a = intensity
		float     radius;
		int       type;     // 0 = Directional, 1 = Point (see Fufu::LightType)
		float     _pad[2];
	};

	struct alignas(16) GPUCamera
	{
		glm::vec4 position;
		glm::vec4 forward;
		glm::vec4 right;
		glm::vec4 up;
		float     fov;
		float     aspectRatio;
		float     nearPlane;
		float     _pad;
	};

	struct alignas(16) GPUFrameData
	{
		int   frameIndex;       // Accumulation counter
		int   maxBounces;
		int   samplesPerPixel;
		float exposure;
		int   width;
		int   height;
		int   triangleCount;
		int   materialCount;
		int   lightCount;
		int   technique; // 0 = PathTracing, 1 = RayTracing (see Fufu::RenderTechnique)
		int   aaMode;         // 0=None, 1=SSAA, 2=TAA, 3=FXAA (see Fufu::AAMode)
		int   taaFrameIndex;  // Counter dedicated to TAA: increments every frame regardless of RenderMode
		float taaBlendFactor;
		int   hasSkybox;       // 1 = sample u_Skybox, 0 = procedural sky gradient
		float skyboxIntensity;
	};

}