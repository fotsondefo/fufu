#include "depch.h"
#include "Renderer/BVH.h"
#include <algorithm>
#include <functional>

namespace Fufu
{
	namespace
	{
		struct AABB
		{
			glm::vec3 min{ 1e30f, 1e30f, 1e30f };
			glm::vec3 max{ -1e30f, -1e30f, -1e30f };

			void grow(const glm::vec3& p)
			{
				min = glm::min(min, p);
				max = glm::max(max, p);
			}

			void grow(const AABB& other)
			{
				grow(other.min);
				grow(other.max);
			}

			glm::vec3 extent() const { return max - min; }
		};

		AABB triangleBounds(const GPUTriangle& tri)
		{
			AABB box;
			box.grow(glm::vec3(tri.v0));
			box.grow(glm::vec3(tri.v1));
			box.grow(glm::vec3(tri.v2));
			return box;
		}

		glm::vec3 triangleCentroid(const GPUTriangle& tri)
		{
			return (glm::vec3(tri.v0) + glm::vec3(tri.v1) + glm::vec3(tri.v2)) / 3.f;
		}
	}

	std::vector<GPUBVHNode> BVHBuilder::build(std::vector<GPUTriangle>& triangles, int leafSize)
	{
		std::vector<GPUBVHNode> nodes;

		if (triangles.empty())
		{
			nodes.push_back(GPUBVHNode{ glm::vec4(0.f), glm::vec4(0.f), -1, -1, 0, 0 });
			return nodes;
		}

		std::size_t triCount = triangles.size();
		std::vector<AABB> bounds(triCount);
		std::vector<glm::vec3> centroids(triCount);
		std::vector<int> indices(triCount);

		for (std::size_t i = 0; i < triCount; ++i)
		{
			bounds[i] = triangleBounds(triangles[i]);
			centroids[i] = triangleCentroid(triangles[i]);
			indices[i] = static_cast<int>(i);
		}

		nodes.reserve(triCount * 2);

		std::function<int(int, int)> buildRecursive = [&](int start, int end) -> int
		{
			AABB nodeBounds;
			for (int i = start; i < end; ++i)
				nodeBounds.grow(bounds[indices[i]]);

			int nodeIndex = static_cast<int>(nodes.size());
			nodes.push_back(GPUBVHNode{});

			int count = end - start;
			if (count <= leafSize)
			{
				nodes[nodeIndex] = GPUBVHNode{
					glm::vec4(nodeBounds.min, 0.f), glm::vec4(nodeBounds.max, 0.f),
					-1, -1, start, count
				};
				return nodeIndex;
			}

			AABB centroidBounds;
			for (int i = start; i < end; ++i)
				centroidBounds.grow(centroids[indices[i]]);

			glm::vec3 extent = centroidBounds.extent();
			int axis = 0;
			if (extent.y > extent.x) axis = 1;
			if (extent.z > extent[axis]) axis = 2;

			int mid = start + count / 2;
			std::nth_element(indices.begin() + start, indices.begin() + mid, indices.begin() + end,
				[&](int a, int b) { return centroids[a][axis] < centroids[b][axis]; });

			int leftIdx = buildRecursive(start, mid);
			int rightIdx = buildRecursive(mid, end);

			nodes[nodeIndex] = GPUBVHNode{
				glm::vec4(nodeBounds.min, 0.f), glm::vec4(nodeBounds.max, 0.f),
				leftIdx, rightIdx, 0, 0
			};
			return nodeIndex;
		};

		buildRecursive(0, static_cast<int>(triCount));

		// Réordonne les triangles selon la permutation finale, une seule fois
		// (plutôt que de trimballer des GPUTriangle ~100 octets pendant la
		// récursion : on ne déplace que des int jusqu'ici).
		std::vector<GPUTriangle> reordered(triCount);
		for (std::size_t i = 0; i < triCount; ++i)
			reordered[i] = triangles[static_cast<std::size_t>(indices[i])];
		triangles = std::move(reordered);

		return nodes;
	}
}
