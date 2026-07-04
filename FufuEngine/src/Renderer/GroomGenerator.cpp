#include "depch.h"
#include "Renderer/GroomGenerator.h"
#include <algorithm>
#include <glm/geometric.hpp>

namespace Fufu
{
	// PRNG xorshift32 simple : déterministe, rapide, suffisant pour une
	// distribution procédurale reproductible par seed (pas cryptographique).
	static float randFloat(uint32_t& state)
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return static_cast<float>(state) / static_cast<float>(0xFFFFFFFFu);
	}

	void GroomGenerator::generate(const MeshAsset& mesh, const GroomComponent& groom,
		std::vector<GPUTriangle>& outTriangles)
	{
		if (mesh.getSubMeshCount() == 0 || groom.strandCount <= 0)
			return;

		const SubMesh& sub = mesh.getSubMeshes()[0];
		std::size_t triCount = sub.indices.size() / 3;
		if (triCount == 0) return;

		// Poids par aire pour une distribution uniforme sur la surface
		std::vector<float> cumulativeArea(triCount);
		float totalArea = 0.f;
		for (std::size_t t = 0; t < triCount; ++t)
		{
			const glm::vec3& a = sub.vertices[sub.indices[t * 3 + 0]].position;
			const glm::vec3& b = sub.vertices[sub.indices[t * 3 + 1]].position;
			const glm::vec3& c = sub.vertices[sub.indices[t * 3 + 2]].position;
			totalArea += 0.5f * glm::length(glm::cross(b - a, c - a));
			cumulativeArea[t] = totalArea;
		}
		if (totalArea <= 0.f) return;

		uint32_t rngState = (groom.seed == 0) ? 1u : groom.seed;
		int segments = std::max(groom.segments, 1);

		for (int s = 0; s < groom.strandCount; ++s)
		{
			float areaPick = randFloat(rngState) * totalArea;
			auto it = std::lower_bound(cumulativeArea.begin(), cumulativeArea.end(), areaPick);
			std::size_t triIdx = static_cast<std::size_t>(it - cumulativeArea.begin());
			triIdx = std::min(triIdx, triCount - 1);

			const Vertex& va = sub.vertices[sub.indices[triIdx * 3 + 0]];
			const Vertex& vb = sub.vertices[sub.indices[triIdx * 3 + 1]];
			const Vertex& vc = sub.vertices[sub.indices[triIdx * 3 + 2]];

			float r1 = randFloat(rngState);
			float r2 = randFloat(rngState);
			if (r1 + r2 > 1.f) { r1 = 1.f - r1; r2 = 1.f - r2; }
			float r0 = 1.f - r1 - r2;

			glm::vec3 rootLocal = r0 * va.position + r1 * vb.position + r2 * vc.position;
			glm::vec3 normalLocal = glm::normalize(r0 * va.normal + r1 * vb.normal + r2 * vc.normal);

			float lengthJitter = 1.f + (randFloat(rngState) * 2.f - 1.f) * groom.randomness;
			float strandLength = groom.length * std::max(lengthJitter, 0.1f);

			glm::vec3 horizontalJitter = glm::vec3(
				randFloat(rngState) * 2.f - 1.f,
				0.f,
				randFloat(rngState) * 2.f - 1.f) * (groom.randomness * 0.5f);

			glm::vec3 dir = glm::normalize(normalLocal + horizontalJitter);
			float step = strandLength / static_cast<float>(segments);

			// Vecteur perpendiculaire à la mèche, pour donner sa largeur au ruban
			glm::vec3 side = glm::cross(dir, glm::vec3(0.f, 1.f, 0.f));
			if (glm::dot(side, side) < 1e-6f)
				side = glm::cross(dir, glm::vec3(1.f, 0.f, 0.f));
			side = glm::normalize(side);

			glm::vec3 prevPos = rootLocal;

			for (int seg = 0; seg < segments; ++seg)
			{
				// Courbure vers le bas (approx. gravité en espace local de l'entité)
				float bend = groom.gravity * (static_cast<float>(seg + 1) / static_cast<float>(segments));
				glm::vec3 segDir = glm::normalize(dir + glm::vec3(0.f, -bend, 0.f));
				glm::vec3 nextPos = prevPos + segDir * step;

				float taperStart = 1.f - static_cast<float>(seg) / static_cast<float>(segments) * 0.7f;
				float taperEnd = 1.f - static_cast<float>(seg + 1) / static_cast<float>(segments) * 0.7f;
				float halfWidthStart = groom.thickness * 0.5f * taperStart;
				float halfWidthEnd = groom.thickness * 0.5f * taperEnd;

				glm::vec3 p0 = prevPos - side * halfWidthStart;
				glm::vec3 p1 = prevPos + side * halfWidthStart;
				glm::vec3 p2 = nextPos + side * halfWidthEnd;
				glm::vec3 p3 = nextPos - side * halfWidthEnd;

				glm::vec3 segNormalLocal = glm::normalize(glm::cross(side, segDir));

				auto emitTriangle = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
				{
					GPUTriangle tri{};
					tri.v0 = glm::vec4(a, 0.f);
					tri.v1 = glm::vec4(b, 0.f);
					tri.v2 = glm::vec4(c, 0.f);
					tri.n0 = glm::vec4(segNormalLocal, 0.f);
					tri.n1 = glm::vec4(segNormalLocal, 0.f);
					tri.n2 = glm::vec4(segNormalLocal, 0.f);
					tri.uv0 = glm::vec2(0.f);
					tri.uv1 = glm::vec2(0.f);
					tri.uv2 = glm::vec2(0.f);
					tri.materialIndex = 0;
					outTriangles.push_back(tri);
				};

				emitTriangle(p0, p1, p2);
				emitTriangle(p0, p2, p3);

				prevPos = nextPos;
				dir = segDir;
			}
		}
	}
}
