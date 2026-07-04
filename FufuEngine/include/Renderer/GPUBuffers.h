#pragma once

#pragma once
#include <glm/glm.hpp>


namespace Fufu 
{

	// Structures miroir c�t� CPU de ce qui est envoy� au GPU via SSBO.
	// Alignement std430 strict � chaque champ respecte les r�gles d'alignement GLSL.

	struct alignas(16) GPUSphere
	{
		glm::vec3 center;
		float     radius;
		int       materialIndex;
		float     _pad[3];
	};

	struct alignas(16) GPUTriangle
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
	};

}