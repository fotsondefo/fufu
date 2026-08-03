#pragma once

#include "GPUBuffers.h"
#include <vector>

namespace Fufu
{
	// GPU-side BVH node (std430, 48 bytes: 2×vec4 + 4×int, already a multiple
	// of 16, no padding needed). Reused for BLAS (leaves = triangles, via
	// firstTri/triCount) AND TLAS (leaves = instances, the same firstTri/triCount
	// fields serving as firstInstance/instCount).
	struct alignas(16) GPUBVHNode
	{
		glm::vec4 boundsMin; // w unused
		glm::vec4 boundsMax; // w unused
		int left;            // index of left child; -1 if leaf
		int right;           // index of right child; -1 if leaf
		int firstTri;        // leaf: start index (triangles OR instances depending on the buffer)
		int triCount;        // leaf: element count; 0 if internal node
	};

	class BVHBuilder
	{
	public:
		// BLAS: builds a BVH using binned SAH (Surface Area Heuristic, see
		// BVH.cpp) on the provided triangles (local space) — a tree noticeably
		// better balanced against the real geometry than a simple median split,
		// which reduces the number of nodes visited per ray during traversal.
		// REORDERS `triangles` in place so each leaf references a contiguous
		// range [firstTri, firstTri+triCount).
		static std::vector<GPUBVHNode> build(std::vector<GPUTriangle>& triangles, int leafSize = 4);

		// TLAS: builds a BVH over bounding boxes provided directly (one per
		// instance, in world space). Also returns the final permutation in
		// `order` (order[i] = original index of the element placed at position i)
		// so the caller can reorder its own parallel array (e.g. the instance
		// buffer) in the same way.
		static std::vector<GPUBVHNode> buildFromBounds(
			const std::vector<glm::vec3>& boundsMin,
			const std::vector<glm::vec3>& boundsMax,
			std::vector<int>& order,
			int leafSize = 1);
	};
}
