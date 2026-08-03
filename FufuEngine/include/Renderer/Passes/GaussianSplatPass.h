#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace Fufu
{

class Scene;

// Per-Gaussian data loaded from a .ply file.
struct RawGaussian
{
    glm::vec3 pos;
    glm::vec4 rot;      // quaternion wxyz
    glm::vec3 scale;    // log-scale (exp before use)
    float     opacity;  // pre-sigmoid
    float     sh[48];   // DC (3) + rest (45) for degree-3 SH
};

// GPU-side splat data (sorted back-to-front each frame).
struct alignas(16) SplatGPU
{
    glm::vec2 screenPos;    // NDC [-1,1]
    float     conic[3];     // elements of inverse 2D cov: a, b, c (upper triangle)
    float     opacity;
    glm::vec3 color;
    float     camDepth;     // camera-space z (for depth test vs gPosTex)
};

// Cached cloud data for one .ply file.
struct GaussianCloud
{
    std::vector<RawGaussian> gaussians;
    bool loaded = false;
};

class GaussianSplatPass
{
public:
    void init    (int width, int height);
    void shutdown();
    void resize  (int width, int height);

    // Returns a new output texture with splats composited over hdrTex.
    // Returns hdrTex unchanged if no GaussianSplatComponents exist in the scene.
    uint32_t execute(Scene& scene,
                     uint32_t hdrTex,
                     uint32_t gPosTex,
                     uint32_t quadVAO,
                     int width, int height,
                     const glm::mat4& view,
                     const glm::mat4& proj,
                     const glm::vec3& camPos,
                     float camFov, float camAspect);

    uint32_t getOutputTexture() const { return m_OutputTex; }

private:
    GaussianCloud& getOrLoadCloud(const std::string& path);
    void projectAndSort(const GaussianCloud& cloud,
                        const glm::mat4& view,
                        const glm::mat4& proj,
                        float camAspect, float camFov,
                        const glm::mat4& worldTransform,
                        float globalOpacity, int shDegree,
                        const glm::vec3& camPos,
                        std::vector<SplatGPU>& out);

    void createTextures(int w, int h);
    void deleteTextures();

    uint32_t m_Program   = 0;
    uint32_t m_OutputTex = 0;
    uint32_t m_FBO       = 0;
    uint32_t m_SSBO      = 0;   // SplatGPU array (sorted, resized as needed)
    uint32_t m_SplatVAO  = 0;   // empty VAO for gl_VertexID expansion
    size_t   m_SSBOSize  = 0;   // current allocation in bytes

    std::unordered_map<std::string, GaussianCloud> m_Clouds;

    // Scratch buffer reused every frame to avoid per-frame allocations.
    std::vector<SplatGPU>   m_SortedSplats;
    std::vector<uint32_t>   m_SortIndices;
};

} // namespace Fufu
