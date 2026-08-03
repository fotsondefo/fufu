#include "depch.h"
#include "Renderer/Passes/GaussianSplatPass.h"
#include "Project/Scene/Scene.h"
#include "Project/Components.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <cmath>

namespace Fufu
{

// ─────────────────────────────────────────────────────────────────────────────
// Shader helpers
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t compileShader(GLenum type, const char* src)
{
    uint32_t id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    int ok; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[2048]; glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        FUFU_ERROR("GaussianSplatPass shader error: {}", log);
    }
    return id;
}

static uint32_t buildProg(const char* vertSrc, const char* fragSrc)
{
    uint32_t v = compileShader(GL_VERTEX_SHADER,   vertSrc);
    uint32_t f = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    uint32_t p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[2048]; glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        FUFU_ERROR("GaussianSplatPass link error: {}", log);
    }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

static std::string readFile(const std::filesystem::path& p)
{
    std::ifstream f(p);
    if (!f) return {};
    return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

// ─────────────────────────────────────────────────────────────────────────────
// PLY parser
// ─────────────────────────────────────────────────────────────────────────────

// Maps a property name to its byte offset within one element.
struct PlyProp { std::string name; size_t offset; };

static bool parsePly(const std::string& path, std::vector<RawGaussian>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { FUFU_ERROR("GaussianSplatPass: cannot open '{}'", path); return false; }

    // ── header ──────────────────────────────────────────────────────────────
    std::string line;
    bool binary_little = false;
    int  numVertices   = 0;
    std::vector<PlyProp> props;
    size_t stride = 0;

    while (std::getline(f, line))
    {
        // Trim trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == "end_header") break;

        if (line.find("format binary_little_endian") != std::string::npos)
            binary_little = true;
        else if (line.rfind("element vertex", 0) == 0)
            numVertices = std::stoi(line.substr(15));
        else if (line.rfind("property float", 0) == 0)
        {
            std::string name = line.substr(15);
            props.push_back({ name, stride });
            stride += sizeof(float);
        }
    }

    if (!binary_little || numVertices == 0 || stride == 0)
    {
        FUFU_ERROR("GaussianSplatPass: '{}' is not a supported binary PLY", path);
        return false;
    }

    // ── build property lookup ─────────────────────────────────────────────
    auto getOff = [&](const std::string& name) -> int
    {
        for (auto& p : props)
            if (p.name == name) return static_cast<int>(p.offset);
        return -1;
    };

    int offX  = getOff("x"),  offY  = getOff("y"),  offZ  = getOff("z");
    int offR0 = getOff("rot_0"), offR1 = getOff("rot_1"),
        offR2 = getOff("rot_2"), offR3 = getOff("rot_3");
    int offS0 = getOff("scale_0"), offS1 = getOff("scale_1"), offS2 = getOff("scale_2");
    int offOp = getOff("opacity");
    int offDC0 = getOff("f_dc_0"), offDC1 = getOff("f_dc_1"), offDC2 = getOff("f_dc_2");

    if (offX < 0 || offR0 < 0 || offS0 < 0 || offOp < 0)
    {
        FUFU_ERROR("GaussianSplatPass: '{}' missing required PLY properties", path);
        return false;
    }

    // Gather rest SH offsets (f_rest_0..f_rest_44)
    int offRest[45];
    int numRest = 0;
    for (int i = 0; i < 45; ++i)
    {
        int o = getOff("f_rest_" + std::to_string(i));
        offRest[i] = o;
        if (o >= 0) ++numRest;
        else break;
    }

    // ── read binary body ──────────────────────────────────────────────────
    std::vector<uint8_t> buf(static_cast<size_t>(numVertices) * stride);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));

    auto getF = [&](int vertex, int byteOff) -> float
    {
        float v;
        std::memcpy(&v, buf.data() + vertex * stride + byteOff, sizeof(float));
        return v;
    };

    out.resize(numVertices);
    for (int i = 0; i < numVertices; ++i)
    {
        RawGaussian& g = out[i];
        g.pos   = { getF(i, offX), getF(i, offY), getF(i, offZ) };
        g.rot   = { getF(i, offR0), getF(i, offR1), getF(i, offR2), getF(i, offR3) };
        g.scale = { getF(i, offS0), getF(i, offS1), getF(i, offS2) };
        g.opacity = getF(i, offOp);
        std::memset(g.sh, 0, sizeof(g.sh));
        if (offDC0 >= 0) g.sh[0] = getF(i, offDC0);
        if (offDC1 >= 0) g.sh[1] = getF(i, offDC1);
        if (offDC2 >= 0) g.sh[2] = getF(i, offDC2);
        for (int r = 0; r < numRest && r < 45; ++r)
            if (offRest[r] >= 0) g.sh[3 + r] = getF(i, offRest[r]);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SH evaluation (view-dependent color)
// ─────────────────────────────────────────────────────────────────────────────

static constexpr float C0 = 0.28209479177387814f;
static constexpr float C1 = 0.4886025119029199f;
static constexpr float C2[5] = { 1.0925484305920792f, -1.0925484305920792f,
                                   0.31539156525252005f, -1.0925484305920792f,
                                   0.5462742152960396f };
static constexpr float C3[7] = {
    -0.5900435899266435f,  2.890611442640554f,   -0.4570457994644658f,
     0.3731763325901154f, -0.4570457994644658f,   1.445305721320277f,
    -0.5900435899266435f
};

static glm::vec3 evalSH(const float* sh, int degree, glm::vec3 dir)
{
    glm::vec3 col(C0 * sh[0], C0 * sh[1], C0 * sh[2]);
    col += 0.5f;
    if (degree < 1) return glm::clamp(col, 0.f, 1.f);

    float x = dir.x, y = dir.y, z = dir.z;
    col += -C1*y * glm::vec3(sh[3], sh[4], sh[5])
         +  C1*z * glm::vec3(sh[6], sh[7], sh[8])
         + -C1*x * glm::vec3(sh[9], sh[10], sh[11]);
    if (degree < 2) return glm::clamp(col, 0.f, 1.f);

    float xx=x*x, yy=y*y, zz=z*z, xy=x*y, yz=y*z, xz=x*z;
    col += C2[0]*xy  * glm::vec3(sh[12],sh[13],sh[14])
         + C2[1]*yz  * glm::vec3(sh[15],sh[16],sh[17])
         + C2[2]*(2.f*zz-xx-yy) * glm::vec3(sh[18],sh[19],sh[20])
         + C2[3]*xz  * glm::vec3(sh[21],sh[22],sh[23])
         + C2[4]*(xx-yy) * glm::vec3(sh[24],sh[25],sh[26]);
    if (degree < 3) return glm::clamp(col, 0.f, 1.f);

    col += C3[0]*y*(3.f*xx-yy) * glm::vec3(sh[27],sh[28],sh[29])
         + C3[1]*xy*z           * glm::vec3(sh[30],sh[31],sh[32])
         + C3[2]*y*(4.f*zz-xx-yy) * glm::vec3(sh[33],sh[34],sh[35])
         + C3[3]*z*(2.f*zz-3.f*xx-3.f*yy) * glm::vec3(sh[36],sh[37],sh[38])
         + C3[4]*x*(4.f*zz-xx-yy) * glm::vec3(sh[39],sh[40],sh[41])
         + C3[5]*z*(xx-yy)     * glm::vec3(sh[42],sh[43],sh[44])
         + C3[6]*x*(xx-3.f*yy) * glm::vec3(sh[45],sh[46],sh[47]);
    return glm::clamp(col, 0.f, 1.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Projection: 3D Gaussian → 2D conic
// ─────────────────────────────────────────────────────────────────────────────

// Returns false if the splat is behind the camera or outside frustum.
static bool project(const RawGaussian& g,
                    const glm::mat4& view,
                    const glm::mat4& proj,
                    float screenW, float screenH,
                    const glm::mat4& world,
                    glm::vec2& ndcPos,
                    float (&conic)[3],
                    float& camDepthOut)
{
    // World-space position
    glm::vec4 wp  = world * glm::vec4(g.pos, 1.f);

    // Camera-space
    glm::vec4 cp  = view * wp;
    if (cp.z >= 0.f) return false; // behind camera
    camDepthOut = -cp.z;

    // Clip / NDC
    glm::vec4 clip = proj * cp;
    if (clip.w <= 0.f) return false;
    glm::vec3 ndc  = glm::vec3(clip) / clip.w;
    if (ndc.x < -1.3f || ndc.x > 1.3f || ndc.y < -1.3f || ndc.y > 1.3f) return false;

    ndcPos = glm::vec2(ndc.x, ndc.y);

    // Build 3D covariance from scale + rotation
    glm::vec3 s = glm::exp(g.scale);
    glm::mat3 S = glm::mat3(0.f);
    S[0][0] = s.x; S[1][1] = s.y; S[2][2] = s.z;

    // rot is stored wxyz — build quaternion
    glm::quat q(g.rot.x, g.rot.y, g.rot.z, g.rot.w); // w=rot.x, x=rot.y, etc.
    glm::mat3 R = glm::mat3_cast(glm::normalize(q));

    glm::mat3 world3 = glm::mat3(world);
    glm::mat3 Rw = world3 * R;

    glm::mat3 cov3d = Rw * S * glm::transpose(S) * glm::transpose(Rw);

    // Jacobian of perspective projection at (cp.x, cp.y, cp.z)
    float fx = screenW * 0.5f;  // approximate focal length in pixels (aspect baked in)
    float fy = screenH * 0.5f;
    float iz = 1.f / cp.z;
    glm::mat3 J(0.f);
    J[0][0] = fx * iz;
    J[1][1] = fy * iz;
    J[2][0] = -fx * cp.x * iz * iz;
    J[2][1] = -fy * cp.y * iz * iz;

    glm::mat3 W = glm::mat3(view);
    glm::mat3 T = J * W;
    glm::mat3 cov2d = T * cov3d * glm::transpose(T);

    // Upper 2×2 + small regularization
    float a = cov2d[0][0] + 0.3f;
    float b = cov2d[0][1];          // = cov2d[1][0]
    float c = cov2d[1][1] + 0.3f;

    // Inverse of 2×2: [a b; b c]^-1 = 1/det * [c -b; -b a]
    float det = a * c - b * b;
    if (det < 1e-10f) return false;
    float idet = 1.f / det;
    conic[0] = c  * idet;
    conic[1] = -b * idet;
    conic[2] = a  * idet;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// GaussianSplatPass implementation
// ─────────────────────────────────────────────────────────────────────────────

GaussianCloud& GaussianSplatPass::getOrLoadCloud(const std::string& path)
{
    auto it = m_Clouds.find(path);
    if (it != m_Clouds.end()) return it->second;

    GaussianCloud& cloud = m_Clouds[path];
    if (!parsePly(path, cloud.gaussians))
    {
        FUFU_WARN("GaussianSplatPass: failed to load '{}'", path);
    }
    else
    {
        FUFU_INFO("GaussianSplatPass: loaded {} gaussians from '{}'",
                  cloud.gaussians.size(), path);
    }
    cloud.loaded = true;
    return cloud;
}

void GaussianSplatPass::projectAndSort(
    const GaussianCloud& cloud,
    const glm::mat4& view,
    const glm::mat4& proj,
    float camAspect, float camFov,
    const glm::mat4& worldTransform,
    float globalOpacity, int shDegree,
    const glm::vec3& camPos,
    std::vector<SplatGPU>& out)
{
    const size_t N = cloud.gaussians.size();

    // Project
    struct Candidate { SplatGPU splat; float depth; };
    std::vector<Candidate> candidates;
    candidates.reserve(N);

    float screenW = 2.f;  // NDC units — Jacobian uses approximate pixel focal lengths
    float screenH = 2.f / camAspect;

    for (const RawGaussian& g : cloud.gaussians)
    {
        glm::vec2 ndc;
        float conic[3], depth;
        if (!project(g, view, proj, screenW, screenH, worldTransform, ndc, conic, depth))
            continue;

        // View direction for SH
        glm::vec4 wp = worldTransform * glm::vec4(g.pos, 1.f);
        glm::vec3 dir = glm::normalize(glm::vec3(wp) - camPos);

        float alpha = 1.f / (1.f + std::exp(-g.opacity)); // sigmoid
        alpha *= globalOpacity;
        if (alpha < 1.f / 255.f) continue;

        SplatGPU s;
        s.screenPos = ndc;
        s.conic[0] = conic[0]; s.conic[1] = conic[1]; s.conic[2] = conic[2];
        s.opacity  = alpha;
        s.color    = evalSH(g.sh, std::min(shDegree, 3), dir);
        s.camDepth = depth;

        candidates.push_back({ s, depth });
    }

    // Sort back-to-front
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b){ return a.depth > b.depth; });

    out.resize(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i)
        out[i] = candidates[i].splat;
}

void GaussianSplatPass::createTextures(int w, int h)
{
    glGenTextures(1, &m_OutputTex);
    glBindTexture(GL_TEXTURE_2D, m_OutputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_OutputTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GaussianSplatPass::deleteTextures()
{
    glDeleteTextures(1, &m_OutputTex);     m_OutputTex = 0;
    glDeleteFramebuffers(1, &m_FBO);       m_FBO = 0;
}

void GaussianSplatPass::init(int width, int height)
{
    // Load shaders from disk
    auto shaderDir = std::filesystem::current_path() / "shaders";
    std::string vertSrc = readFile(shaderDir / "GaussianSplat.vert");
    std::string fragSrc = readFile(shaderDir / "GaussianSplat.frag");
    if (vertSrc.empty() || fragSrc.empty())
    {
        FUFU_ERROR("GaussianSplatPass: shader files not found");
        return;
    }
    m_Program = buildProg(vertSrc.c_str(), fragSrc.c_str());

    // SSBO for splat data
    glGenBuffers(1, &m_SSBO);

    // Empty VAO (vertex data comes from SSBO via gl_InstanceID)
    glGenVertexArrays(1, &m_SplatVAO);

    createTextures(width, height);
}

void GaussianSplatPass::shutdown()
{
    glDeleteProgram(m_Program);   m_Program = 0;
    glDeleteBuffers(1, &m_SSBO);  m_SSBO = 0;
    glDeleteVertexArrays(1, &m_SplatVAO); m_SplatVAO = 0;
    deleteTextures();
    m_Clouds.clear();
}

void GaussianSplatPass::resize(int w, int h)
{
    deleteTextures();
    createTextures(w, h);
}

uint32_t GaussianSplatPass::execute(
    Scene& scene,
    uint32_t hdrTex, uint32_t gPosTex,
    uint32_t /*quadVAO*/,
    int width, int height,
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& camPos,
    float camFov, float camAspect)
{
    if (!m_Program) return hdrTex;

    // Collect all GaussianSplatComponents
    auto& reg = scene.getRegistry();
    auto v = reg.view<TransformComponent, GaussianSplatComponent>();
    if (v.begin() == v.end()) return hdrTex;

    // Build sorted splat list across all components
    m_SortedSplats.clear();
    std::vector<SplatGPU> partial;

    for (auto [ent, tc, gs] : v.each())
    {
        if (gs.path.empty()) continue;
        GaussianCloud& cloud = getOrLoadCloud(gs.path);
        if (cloud.gaussians.empty()) continue;

        glm::mat4 world = tc.getTransform();
        partial.clear();
        projectAndSort(cloud, view, proj, camAspect, camFov,
                       world, gs.opacity, gs.shDegree, camPos, partial);

        m_SortedSplats.insert(m_SortedSplats.end(), partial.begin(), partial.end());
    }

    if (m_SortedSplats.empty()) return hdrTex;

    // Sort merged list (multiple components interleaved by depth)
    std::stable_sort(m_SortedSplats.begin(), m_SortedSplats.end(),
        [](const SplatGPU& a, const SplatGPU& b){ return a.camDepth > b.camDepth; });

    // Upload to SSBO
    const size_t dataSize = m_SortedSplats.size() * sizeof(SplatGPU);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
    if (dataSize > m_SSBOSize)
    {
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(dataSize),
                     m_SortedSplats.data(), GL_DYNAMIC_DRAW);
        m_SSBOSize = dataSize;
    }
    else
    {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        static_cast<GLsizeiptr>(dataSize), m_SortedSplats.data());
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_SSBO);

    // Copy hdrTex into m_OutputTex
    glCopyImageSubData(hdrTex, GL_TEXTURE_2D, 0, 0, 0, 0,
                       m_OutputTex, GL_TEXTURE_2D, 0, 0, 0, 0,
                       width, height, 1);

    // Render splats into m_FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);

    // Premultiplied alpha: src=ONE, dst=ONE_MINUS_SRC_ALPHA
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_Program);

    // Bind gPosTex for depth comparison
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosTex);
    glUniform1i(glGetUniformLocation(m_Program, "u_GPosTex"), 0);

    glUniform2f(glGetUniformLocation(m_Program, "u_ScreenSize"),
                static_cast<float>(width), static_cast<float>(height));
    glUniform3fv(glGetUniformLocation(m_Program, "u_CamPos"), 1, glm::value_ptr(camPos));
    glUniformMatrix4fv(glGetUniformLocation(m_Program, "u_View"),
                       1, GL_FALSE, glm::value_ptr(view));

    glBindVertexArray(m_SplatVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0,
                          6, static_cast<GLsizei>(m_SortedSplats.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_OutputTex;
}

} // namespace Fufu
