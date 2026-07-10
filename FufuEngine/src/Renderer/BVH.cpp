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
			glm::vec3 center() const { return (min + max) * 0.5f; }
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

		// Coût SAH d'un (sous-)ensemble de primitives regroupées sous `bounds` :
		// proportionnel à surface(bounds) * count. Le facteur 2 habituel de
		// l'aire de surface est omis (constant, inutile pour comparer des coûts
		// entre eux) — même principe que jbikker/pbrt.
		float sahCost(const AABB& bounds, int count)
		{
			if (count == 0) return 0.f;
			glm::vec3 e = bounds.extent();
			float halfArea = e.x * e.y + e.y * e.z + e.z * e.x;
			return halfArea * static_cast<float>(count);
		}

		constexpr int k_SAHBins = 8;

		struct SAHBin
		{
			AABB bounds;
			int  count = 0;
		};

		// Cœur générique du BVH, partagé par le BLAS (triangles) et le TLAS
		// (boîtes d'instances) : opère uniquement sur bounds/centroids et une
		// permutation d'indices, sans connaître la nature des éléments.
		//
		// Split par SAH binné (approximation "fast SAH" à la jbikker/pbrt) :
		// teste k_SAHBins-1 plans de coupe candidats par axe (O(n) par axe via
		// des bins, pas O(n log n) via un tri exact) et garde le moins coûteux
		// des 3 axes. Bien plus rentable qu'un split médian sur des maillages
		// de plusieurs millions de triangles — un arbre mieux équilibré par
		// rapport à la géométrie réelle réduit directement le nombre de nœuds
		// visités par rayon pendant la traversée GPU.
		std::vector<GPUBVHNode> buildGeneric(
			const std::vector<AABB>& bounds,
			const std::vector<glm::vec3>& centroids,
			std::vector<int>& indices,
			int leafSize)
		{
			std::vector<GPUBVHNode> nodes;

			if (indices.empty())
			{
				nodes.push_back(GPUBVHNode{ glm::vec4(0.f), glm::vec4(0.f), -1, -1, 0, 0 });
				return nodes;
			}

			nodes.reserve(indices.size() * 2);

			auto makeLeaf = [&](int nodeIndex, const AABB& nodeBounds, int start, int count)
			{
				nodes[nodeIndex] = GPUBVHNode{
					glm::vec4(nodeBounds.min, 0.f), glm::vec4(nodeBounds.max, 0.f),
					-1, -1, start, count
				};
			};

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
					makeLeaf(nodeIndex, nodeBounds, start, count);
					return nodeIndex;
				}

				AABB centroidBounds;
				for (int i = start; i < end; ++i)
					centroidBounds.grow(centroids[indices[i]]);

				glm::vec3 extent = centroidBounds.extent();

				int   bestAxis = -1;
				float bestSplit = 0.f;
				float bestCost = sahCost(nodeBounds, count); // référence : ne pas splitter du tout

				for (int axis = 0; axis < 3; ++axis)
				{
					if (extent[axis] < 1e-8f) continue; // centroïdes confondus sur cet axe

					SAHBin bins[k_SAHBins];
					float binScale = static_cast<float>(k_SAHBins) / extent[axis];

					for (int i = start; i < end; ++i)
					{
						int idx = indices[i];
						int binIdx = static_cast<int>((centroids[idx][axis] - centroidBounds.min[axis]) * binScale);
						binIdx = std::clamp(binIdx, 0, k_SAHBins - 1);
						bins[binIdx].bounds.grow(bounds[idx]);
						++bins[binIdx].count;
					}

					// Balayage gauche->droite puis droite->gauche : coût cumulé
					// de chaque côté de chaque plan de coupe en O(bins) au lieu
					// de recalculer les AABB pour chaque candidat (O(bins²)).
					AABB leftBounds[k_SAHBins - 1];
					int  leftCount[k_SAHBins - 1];
					AABB rightBounds[k_SAHBins - 1];
					int  rightCount[k_SAHBins - 1];

					AABB accBounds;
					int  accCount = 0;
					for (int i = 0; i < k_SAHBins - 1; ++i)
					{
						accBounds.grow(bins[i].bounds);
						accCount += bins[i].count;
						leftBounds[i] = accBounds;
						leftCount[i] = accCount;
					}

					accBounds = AABB{};
					accCount = 0;
					for (int i = k_SAHBins - 1; i > 0; --i)
					{
						accBounds.grow(bins[i].bounds);
						accCount += bins[i].count;
						rightBounds[i - 1] = accBounds;
						rightCount[i - 1] = accCount;
					}

					for (int i = 0; i < k_SAHBins - 1; ++i)
					{
						if (leftCount[i] == 0 || rightCount[i] == 0) continue;

						float cost = sahCost(leftBounds[i], leftCount[i]) + sahCost(rightBounds[i], rightCount[i]);
						if (cost < bestCost)
						{
							bestCost = cost;
							bestAxis = axis;
							bestSplit = centroidBounds.min[axis] + extent[axis] * (static_cast<float>(i + 1) / k_SAHBins);
						}
					}
				}

				// Aucun plan de coupe n'améliore le coût (géométrie très groupée
				// ou plate) : feuille, même au-delà de leafSize — reste correct,
				// juste sous-optimal pour ce cas rare plutôt que de forcer un
				// split arbitraire qui n'aiderait pas.
				if (bestAxis < 0)
				{
					makeLeaf(nodeIndex, nodeBounds, start, count);
					return nodeIndex;
				}

				int axis = bestAxis;
				auto midIt = std::partition(indices.begin() + start, indices.begin() + end,
					[&](int idx) { return centroids[idx][axis] < bestSplit; });
				int mid = static_cast<int>(midIt - indices.begin());

				// Partition dégénérée (doublons de centroïde pile sur la
				// frontière) : repli sur un split médian pour garantir que la
				// récursion progresse toujours.
				if (mid == start || mid == end)
				{
					mid = start + count / 2;
					std::nth_element(indices.begin() + start, indices.begin() + mid, indices.begin() + end,
						[&](int a, int b) { return centroids[a][axis] < centroids[b][axis]; });
				}

				int leftIdx = buildRecursive(start, mid);
				int rightIdx = buildRecursive(mid, end);

				nodes[nodeIndex] = GPUBVHNode{
					glm::vec4(nodeBounds.min, 0.f), glm::vec4(nodeBounds.max, 0.f),
					leftIdx, rightIdx, 0, 0
				};
				return nodeIndex;
			};

			buildRecursive(0, static_cast<int>(indices.size()));
			return nodes;
		}
	}

	std::vector<GPUBVHNode> BVHBuilder::build(std::vector<GPUTriangle>& triangles, int leafSize)
	{
		if (triangles.empty())
		{
			std::vector<AABB> emptyBounds;
			std::vector<glm::vec3> emptyCentroids;
			std::vector<int> emptyIndices;
			return buildGeneric(emptyBounds, emptyCentroids, emptyIndices, leafSize);
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

		std::vector<GPUBVHNode> nodes = buildGeneric(bounds, centroids, indices, leafSize);

		// Réordonne les triangles selon la permutation finale, une seule fois
		// (plutôt que de trimballer des GPUTriangle ~100 octets pendant la
		// récursion : on ne déplace que des int jusqu'ici).
		std::vector<GPUTriangle> reordered(triCount);
		for (std::size_t i = 0; i < triCount; ++i)
			reordered[i] = triangles[static_cast<std::size_t>(indices[i])];
		triangles = std::move(reordered);

		return nodes;
	}

	std::vector<GPUBVHNode> BVHBuilder::buildFromBounds(
		const std::vector<glm::vec3>& boundsMin,
		const std::vector<glm::vec3>& boundsMax,
		std::vector<int>& order,
		int leafSize)
	{
		std::size_t count = boundsMin.size();

		std::vector<AABB> bounds(count);
		std::vector<glm::vec3> centroids(count);
		std::vector<int> indices(count);

		for (std::size_t i = 0; i < count; ++i)
		{
			bounds[i].min = boundsMin[i];
			bounds[i].max = boundsMax[i];
			centroids[i] = bounds[i].center();
			indices[i] = static_cast<int>(i);
		}

		std::vector<GPUBVHNode> nodes = buildGeneric(bounds, centroids, indices, leafSize);
		order = std::move(indices);
		return nodes;
	}
}
