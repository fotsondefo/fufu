#pragma once

#pragma once
#include <glm/glm.hpp>
#include <vector>


namespace Fufu
{

	// Nombre maximum de textures albedo distinctes bindables en une frame
	// (unités de texture 1..kMaxMaterialTextures, l'unité 0 étant réservée au
	// skybox — voir ComputePass). Doit correspondre à la taille du tableau
	// `u_MaterialTextures[]` dans PathTracer.comp.
	constexpr int kMaxMaterialTextures = 16;

	// Structures miroir c�t� CPU de ce qui est envoy� au GPU via SSBO.
	// Alignement std430 strict � chaque champ respecte les r�gles d'alignement GLSL.

	struct alignas(16) GPUSphere
	{
		glm::vec3 center;
		float     radius;
		int       materialIndex;
		float     _pad[3];
	};

	// Format CPU intermédiaire (PAS un layout SSBO) : sert uniquement pendant
	// la triangulation + BVHBuilder::build (qui réordonne ce vecteur en place
	// pour que chaque feuille référence une plage contiguë). Une fois le BVH
	// construit, splitTriangleBuffers() éclate ce format combiné en les deux
	// buffers GPU réellement uploadés — voir GPUTrianglePosition/GPUTriangleAttribute.
	struct GPUTriangle
	{
		glm::vec4 v0;           // xyz = position, w = unused
		glm::vec4 v1;
		glm::vec4 v2;
		glm::vec4 n0;           // normales par vertex
		glm::vec4 n1;
		glm::vec4 n2;
		glm::vec2 uv0;
		glm::vec2 uv1;
		glm::vec2 uv2;
		int       materialIndex;
		float     _pad;
	};

	// Buffer d'INTERSECTION : seul buffer lu par la traversée du BVH (boucle
	// interne du path/ray tracer). 48 octets, alignement std430 déjà naturel
	// (3x vec4) — pas de normales/UV/matériau ici, pour réduire le trafic
	// mémoire pendant la partie la plus chaude du compute shader.
	struct alignas(16) GPUTrianglePosition
	{
		glm::vec4 v0; // xyz = position, w inutilisé
		glm::vec4 v1;
		glm::vec4 v2;
	};

	// Buffer d'ATTRIBUTS : lu UNE seule fois, après avoir trouvé le point
	// d'impact final du rayon (voir resolveHit() côté shader) — jamais
	// pendant la traversée. materialIndex est un vestige : actuellement
	// toujours écrasé par celui de l'instance (un BLAS peut être partagé par
	// plusieurs instances aux matériaux différents), gardé pour un futur
	// support du multi-matériau par mesh.
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

	// Éclate le format CPU combiné (post-BVHBuilder::build, donc déjà dans
	// l'ordre des feuilles) en les deux buffers GPU — mêmes indices des deux
	// côtés, donc resolveHit() peut réutiliser l'index trouvé pendant la
	// traversée par position sans recalcul.
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

	// Une instance = un exemplaire d'un BLAS (mesh en espace local) avec sa
	// propre transform et son propre matériau. blasNodeOffset/blasTriOffset
	// pointent dans les buffers partagés BLASNodeBuffer/TriangleBuffer, qui
	// concatènent les BLAS de tous les meshes uniques de la scène.
	struct alignas(16) GPUInstance
	{
		glm::mat4 transform;    // local -> monde
		glm::mat4 invTransform; // monde -> local (pour transformer le rayon)
		int materialIndex;
		int blasNodeOffset;
		int blasTriOffset;
		int _pad;
	};

	struct alignas(16) GPUMaterial
	{
		glm::vec4 albedo;       // rgb = couleur, a = opacit�
		float     metallic;
		float     roughness;
		float     emissive;
		float     ior;          // Indice de r�fraction (verre = 1.5)
		int       albedoTexIdx; // -1 = pas de texture
		float     _pad[3];
	};

	// Directional : positionOrDirection.xyz = direction allant d'une surface
	// VERS la lumière (normalisé), radius = rayon angulaire (rad).
	// Point     : positionOrDirection.xyz = position monde, radius = rayon
	// physique de la source (unités monde) — atténuation 1/distance² gérée
	// côté shader, pas ici.
	struct alignas(16) GPULight
	{
		glm::vec4 positionOrDirection;
		glm::vec4 color;    // rgb = couleur, a = intensité
		float     radius;
		int       type;     // 0 = Directional, 1 = Point (voir Fufu::LightType)
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
		int   frameIndex;       // Compteur d'accumulation
		int   maxBounces;
		int   samplesPerPixel;
		float exposure;
		int   width;
		int   height;
		int   triangleCount;
		int   materialCount;
		int   lightCount;
		int   technique; // 0 = PathTracing, 1 = RayTracing (voir Fufu::RenderTechnique)
		int   aaMode;         // 0=None, 1=SSAA, 2=TAA, 3=FXAA (voir Fufu::AAMode)
		int   taaFrameIndex;  // Compteur dédié au TAA : incrémente à chaque frame quel que soit le RenderMode
		float taaBlendFactor;
		int   hasSkybox;       // 1 = échantillonner u_Skybox, 0 = dégradé de ciel procédural
		float skyboxIntensity;
	};

}