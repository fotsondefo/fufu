#pragma once
#include <Renderer/GPUBuffers.h>
#include <Renderer/BVH.h>
#include <Project/Components.h>
#include <Project/Scene/Scene.h>
#include <imgui.h>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

// Utilitaires 3D -> 2D partagés par les démos de visualisation.
namespace FufuLab { namespace Viz
{

// Matrice view-proj depuis la caméra primaire de la scène.
inline glm::mat4 getViewProj(Fufu::Scene& scene, float aspect)
{
    Fufu::Entity cam = scene.getPrimaryCamera();
    if (!cam) return glm::mat4(1.f);
    auto& t = cam.getComponent<Fufu::TransformComponent>();
    auto& c = cam.getComponent<Fufu::CameraComponent>();
    glm::quat rot  = glm::quat(glm::vec3(t.rotation.x, t.rotation.y, 0.f));
    glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1.f), t.position) * glm::mat4_cast(rot));
    return c.getProjectionMatrix(aspect) * view;
}

// Projection world-space -> coordonnées ImGui écran.
// Retourne {-99999, -99999} si le point est derrière la caméra.
inline ImVec2 project(glm::vec3 world, const glm::mat4& vp, ImVec2 origin, ImVec2 sz)
{
    glm::vec4 clip = vp * glm::vec4(world, 1.f);
    if (clip.w <= 0.001f) return { -99999.f, -99999.f };
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return { origin.x + (ndc.x * 0.5f + 0.5f) * sz.x,
             origin.y + (1.f - (ndc.y * 0.5f + 0.5f)) * sz.y };
}

// Fil de fer d'une AABB déjà en espace monde.
inline void drawAABB(ImDrawList* dl,
                     glm::vec3 mn, glm::vec3 mx,
                     const glm::mat4& vp, ImVec2 origin, ImVec2 sz,
                     ImU32 color, float thick = 1.f)
{
    glm::vec3 corners[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}
    };
    ImVec2 p[8];
    for (int i = 0; i < 8; i++) p[i] = project(corners[i], vp, origin, sz);

    static const int E[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (auto& e : E) {
        if (p[e[0]].x < -9000.f || p[e[1]].x < -9000.f) continue;
        dl->AddLine(p[e[0]], p[e[1]], color, thick);
    }
}

// Fil de fer d'une AABB en espace local, transformée par modelMat.
inline void drawAABBLocal(ImDrawList* dl,
                           glm::vec3 mn, glm::vec3 mx,
                           const glm::mat4& modelMat, const glm::mat4& vp,
                           ImVec2 origin, ImVec2 sz,
                           ImU32 color, float thick = 1.f)
{
    glm::vec3 lc[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}
    };
    ImVec2 p[8];
    for (int i = 0; i < 8; i++)
        p[i] = project(glm::vec3(modelMat * glm::vec4(lc[i], 1.f)), vp, origin, sz);

    static const int E[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (auto& e : E) {
        if (p[e[0]].x < -9000.f || p[e[1]].x < -9000.f) continue;
        dl->AddLine(p[e[0]], p[e[1]], color, thick);
    }
}

// Couleur par profondeur : vert (superficiel) → rouge (profond).
inline ImU32 depthColor(int depth, int maxDepth, uint8_t alpha = 200)
{
    float t = (maxDepth > 0) ? std::min(1.f, (float)depth / (float)maxDepth) : 0.f;
    return IM_COL32(
        (uint8_t)(40  + t * 215),
        (uint8_t)(215 - t * 175),
        40,
        alpha
    );
}

} } // namespace FufuLab::Viz
