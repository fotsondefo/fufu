#pragma once
#include <glm/glm.hpp>

namespace Fufu
{

// Clip-space frustum extracted from a view-projection matrix.
// testAABB uses the separating-axis approach: the AABB is outside the frustum
// only if ALL 8 corners are on the negative side of at least one plane.
// Returns true when the AABB is *possibly* visible (conservative — no false
// negatives, possible false positives near plane corners). This is the standard
// fast frustum-cull used in realtime engines.
struct Frustum
{
    glm::vec4 planes[6]; // xyz = plane normal (world-space), w = distance

    // Build from a combined view-projection matrix (row-major GLM convention).
    static Frustum fromViewProj(const glm::mat4& vp)
    {
        Frustum f;
        // Gribb/Hartmann extraction:
        // Left:   row3 + row0
        f.planes[0] = glm::vec4(vp[0][3] + vp[0][0],
                                vp[1][3] + vp[1][0],
                                vp[2][3] + vp[2][0],
                                vp[3][3] + vp[3][0]);
        // Right:  row3 - row0
        f.planes[1] = glm::vec4(vp[0][3] - vp[0][0],
                                vp[1][3] - vp[1][0],
                                vp[2][3] - vp[2][0],
                                vp[3][3] - vp[3][0]);
        // Bottom: row3 + row1
        f.planes[2] = glm::vec4(vp[0][3] + vp[0][1],
                                vp[1][3] + vp[1][1],
                                vp[2][3] + vp[2][1],
                                vp[3][3] + vp[3][1]);
        // Top:    row3 - row1
        f.planes[3] = glm::vec4(vp[0][3] - vp[0][1],
                                vp[1][3] - vp[1][1],
                                vp[2][3] - vp[2][1],
                                vp[3][3] - vp[3][1]);
        // Near:   row3 + row2
        f.planes[4] = glm::vec4(vp[0][3] + vp[0][2],
                                vp[1][3] + vp[1][2],
                                vp[2][3] + vp[2][2],
                                vp[3][3] + vp[3][2]);
        // Far:    row3 - row2
        f.planes[5] = glm::vec4(vp[0][3] - vp[0][2],
                                vp[1][3] - vp[1][2],
                                vp[2][3] - vp[2][2],
                                vp[3][3] - vp[3][2]);

        // Normalize (so the w component is the signed distance from origin)
        for (auto& p : f.planes)
        {
            float len = glm::length(glm::vec3(p));
            if (len > 1e-6f) p /= len;
        }
        return f;
    }

    // Returns false only when the AABB is definitively outside the frustum.
    bool testAABB(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const
    {
        for (const auto& plane : planes)
        {
            // The positive-vertex is the AABB corner furthest along the plane normal.
            glm::vec3 positive(
                plane.x >= 0.f ? aabbMax.x : aabbMin.x,
                plane.y >= 0.f ? aabbMax.y : aabbMin.y,
                plane.z >= 0.f ? aabbMax.z : aabbMin.z);

            if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.f)
                return false; // entirely outside this plane
        }
        return true;
    }
};

} // namespace Fufu
