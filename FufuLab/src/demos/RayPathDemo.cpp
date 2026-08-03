#include "RayPathDemo.h"
#include "VisualizationHelpers.h"
#include "Demo/DemoRegistry.h"
#include <Application/Application.h>
#include <Renderer/Renderer.h>
#include <Project/Components.h>
#include <imgui.h>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

FUFULAB_REGISTER_DEMO("Rendering", "Ray Path", FufuLab::RayPathDemo);

namespace FufuLab
{

// -----------------------------------------------------------------------
// Intersections géométriques
// -----------------------------------------------------------------------

bool RayPathDemo::intersectAABB(glm::vec3 ro, glm::vec3 rd,
                                 glm::vec3 mn, glm::vec3 mx, float& t)
{
    glm::vec3 invD = 1.f / rd;
    glm::vec3 t0   = (mn - ro) * invD;
    glm::vec3 t1   = (mx - ro) * invD;
    glm::vec3 tMin = glm::min(t0, t1);
    glm::vec3 tMax = glm::max(t0, t1);
    float near = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
    float far  = glm::min(glm::min(tMax.x, tMax.y), tMax.z);
    if (far < near || far < 0.f) return false;
    t = near > 0.f ? near : far;
    return true;
}

bool RayPathDemo::intersectTriangle(glm::vec3 ro, glm::vec3 rd,
                                     glm::vec3 v0, glm::vec3 v1, glm::vec3 v2,
                                     float& t)
{
    glm::vec3 e1 = v1 - v0, e2 = v2 - v0;
    glm::vec3 h  = glm::cross(rd, e2);
    float a = glm::dot(e1, h);
    if (glm::abs(a) < 1e-7f) return false;
    float f = 1.f / a;
    glm::vec3 s = ro - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.f || u > 1.f) return false;
    glm::vec3 q = glm::cross(s, e1);
    float v = f * glm::dot(rd, q);
    if (v < 0.f || u + v > 1.f) return false;
    t = f * glm::dot(e2, q);
    return t > 1e-5f;
}

void RayPathDemo::traverseBLAS(glm::vec3 ro, glm::vec3 rd,
                                int nodeOffset, int triOffset,
                                const std::vector<Fufu::GPUBVHNode>&          nodes,
                                const std::vector<Fufu::GPUTrianglePosition>& tris,
                                float& closest, int& hitTri)
{
    int stack[64]; int sp = 0;
    stack[sp++] = 0; // indice relatif de la racine BLAS

    while (sp > 0)
    {
        int relIdx = stack[--sp];
        int absIdx = nodeOffset + relIdx;
        if (absIdx < 0 || absIdx >= (int)nodes.size()) continue;
        const auto& node = nodes[absIdx];

        float tHit;
        if (!intersectAABB(ro, rd, glm::vec3(node.boundsMin), glm::vec3(node.boundsMax), tHit)
            || tHit >= closest)
            continue;

        if (node.triCount > 0)
        {
            // Feuille : tester chaque triangle de la feuille
            for (int i = 0; i < node.triCount; i++)
            {
                int gi = triOffset + node.firstTri + i;
                if (gi < 0 || gi >= (int)tris.size()) continue;
                const auto& tri = tris[gi];
                float t;
                if (intersectTriangle(ro, rd,
                        glm::vec3(tri.v0), glm::vec3(tri.v1), glm::vec3(tri.v2), t)
                    && t < closest)
                {
                    closest = t;
                    hitTri  = gi;
                }
            }
        }
        else
        {
            if (node.left  >= 0) stack[sp++] = node.left;
            if (node.right >= 0) stack[sp++] = node.right;
        }
    }
}

// -----------------------------------------------------------------------
// Traversée TLAS -> BLAS complète
// -----------------------------------------------------------------------

RayPathDemo::HitResult RayPathDemo::castRay(glm::vec3 origin, glm::vec3 dir) const
{
    HitResult result;
    auto& gpu   = Fufu::Application::get().getRenderer().getGPUScene();
    const auto& tlas    = gpu.getTLASNodes();
    const auto& blas    = gpu.getBLASNodes();
    const auto& insts   = gpu.getInstances();
    const auto& triPos  = gpu.getTrianglePositions();

    if (tlas.empty()) return result;

    float closest = 1e30f;
    int   hitTri  = -1;
    int   hitInst = -1;

    int stack[64]; int sp = 0;
    stack[sp++] = 0;

    while (sp > 0)
    {
        int nodeIdx = stack[--sp];
        if (nodeIdx < 0 || nodeIdx >= (int)tlas.size()) continue;
        const auto& node = tlas[nodeIdx];

        float tHit;
        if (!intersectAABB(origin, dir,
                glm::vec3(node.boundsMin), glm::vec3(node.boundsMax), tHit)
            || tHit >= closest)
            continue;

        if (node.triCount > 0)
        {
            // Feuille TLAS = une ou plusieurs instances
            for (int i = 0; i < node.triCount; i++)
            {
                int instIdx = node.firstTri + i;
                if (instIdx < 0 || instIdx >= (int)insts.size()) continue;
                const auto& inst = insts[instIdx];

                // Transformer le rayon en espace local de l'instance
                glm::vec3 localO = glm::vec3(inst.invTransform * glm::vec4(origin, 1.f));
                glm::vec3 localD = glm::vec3(inst.invTransform * glm::vec4(dir,    0.f));

                float localClosest = 1e30f;
                int   localHit     = -1;
                traverseBLAS(localO, localD,
                             inst.blasNodeOffset, inst.blasTriOffset,
                             blas, triPos,
                             localClosest, localHit);

                if (localHit >= 0)
                {
                    // Reconvertir le point d'impact en espace monde pour comparer
                    glm::vec3 localPt = localO + localD * localClosest;
                    glm::vec3 worldPt = glm::vec3(inst.transform * glm::vec4(localPt, 1.f));
                    float worldT = glm::length(worldPt - origin);

                    if (worldT < closest)
                    {
                        closest = worldT;
                        hitTri  = localHit;
                        hitInst = instIdx;
                    }
                }
            }
        }
        else
        {
            if (node.left  >= 0) stack[sp++] = node.left;
            if (node.right >= 0) stack[sp++] = node.right;
        }
    }

    if (hitTri < 0) return result;

    result.hit           = true;
    result.t             = closest;
    result.position      = origin + dir * closest;
    result.instanceIndex = hitInst;
    result.triIndex      = hitTri;

    // Normale géométrique du triangle (espace local → espace monde)
    const auto& inst = insts[hitInst];
    if (hitTri < (int)triPos.size())
    {
        glm::vec3 v0 = glm::vec3(triPos[hitTri].v0);
        glm::vec3 v1 = glm::vec3(triPos[hitTri].v1);
        glm::vec3 v2 = glm::vec3(triPos[hitTri].v2);
        glm::vec3 localN = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        // Normale monde = transpose(inverse(M)) * localN
        glm::mat3 normalMat = glm::transpose(glm::mat3(inst.invTransform));
        result.normal = glm::normalize(normalMat * localN);
        // Orienter vers le rayon entrant
        if (glm::dot(result.normal, dir) > 0.f) result.normal = -result.normal;
    }

    return result;
}

// -----------------------------------------------------------------------
// Traçage d'un pixel
// -----------------------------------------------------------------------

void RayPathDemo::clearPath()
{
    m_Path.clear();
    m_HasPath    = false;
    m_LastPixel[0] = m_LastPixel[1] = -1;
}

void RayPathDemo::traceFromPixel(int px, int py, int viewW, int viewH)
{
    clearPath();
    if (!m_Scene) return;

    Fufu::Entity cam = m_Scene->getPrimaryCamera();
    if (!cam) return;

    auto& t = cam.getComponent<Fufu::TransformComponent>();
    auto& c = cam.getComponent<Fufu::CameraComponent>();

    glm::quat rot   = glm::quat(glm::vec3(t.rotation.x, t.rotation.y, 0.f));
    glm::vec3 fwd   = rot * glm::vec3( 0.f, 0.f, -1.f);
    glm::vec3 right = rot * glm::vec3( 1.f, 0.f,  0.f);
    glm::vec3 up    = rot * glm::vec3( 0.f, 1.f,  0.f);

    float aspect   = (float)viewW / (float)viewH;
    float halfTan  = glm::tan(c.fov * 0.5f);
    float ndcX     = 2.f * (px + 0.5f) / viewW  - 1.f;
    float ndcY     = 1.f - 2.f * (py + 0.5f) / viewH;
    glm::vec3 dir  = glm::normalize(fwd + ndcX * aspect * halfTan * right
                                        + ndcY          * halfTan * up);
    glm::vec3 pos  = t.position;

    constexpr float BIAS = 1e-4f; // offset anti-auto-intersection

    for (int bounce = 0; bounce <= m_MaxBounces; bounce++)
    {
        auto hit = castRay(pos, dir);

        if (!hit.hit)
        {
            // Rayon qui s'échappe vers l'infini (100 unités pour l'affichage)
            m_Path.push_back({ pos, pos + dir * 100.f, {}, false, bounce });
            break;
        }

        m_Path.push_back({ pos, hit.position, hit.normal, true, bounce });

        // Rebond : réflexion miroir sur la normale géométrique
        dir = glm::normalize(glm::reflect(dir, hit.normal));
        pos = hit.position + hit.normal * BIAS;
    }

    m_HasPath      = true;
    m_LastPixel[0] = px;
    m_LastPixel[1] = py;
}

// -----------------------------------------------------------------------
// UI
// -----------------------------------------------------------------------

void RayPathDemo::onParametersPanel()
{
    ImGui::TextDisabled("Traçage de rayon");
    ImGui::Spacing();
    ImGui::SliderInt("Rebonds max",      &m_MaxBounces, 0, 10);
    ImGui::Checkbox("Normales",          &m_ShowNormals);
    ImGui::Checkbox("Points d'impact",   &m_ShowDots);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_HasPath)
    {
        ImGui::TextDisabled("Cliquer dans le viewport");
        ImGui::TextDisabled("pour tracer un rayon.");
        return;
    }

    ImGui::Text("Segments : %d", (int)m_Path.size());
    ImGui::Spacing();

    for (int i = 0; i < (int)m_Path.size(); i++)
    {
        const auto& s = m_Path[i];
        if (s.hasHit)
        {
            float dist = glm::length(s.to - s.from);
            ImGui::Text("  [%d] d=%.3f  (%.2f, %.2f, %.2f)",
                i, dist, s.to.x, s.to.y, s.to.z);
        }
        else
        {
            ImGui::TextDisabled("  [%d] échappe (no hit)", i);
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Effacer")) clearPath();
}

// -----------------------------------------------------------------------
// Viewport overlay
// -----------------------------------------------------------------------

void RayPathDemo::onViewportOverlay(ImDrawList* dl, ImVec2 origin, ImVec2 sz)
{
    if (!m_Scene || sz.x <= 0.f) return;

    ImVec2 mouse  = ImGui::GetMousePos();
    bool   inView = mouse.x >= origin.x && mouse.x < origin.x + sz.x &&
                    mouse.y >= origin.y && mouse.y < origin.y + sz.y;

    // Réticule à la souris quand elle est dans le viewport
    if (inView)
    {
        const ImU32 crossColor = IM_COL32(255, 255, 100, 140);
        dl->AddLine({ mouse.x - 12, mouse.y }, { mouse.x + 12, mouse.y }, crossColor, 1.f);
        dl->AddLine({ mouse.x, mouse.y - 12 }, { mouse.x, mouse.y + 12 }, crossColor, 1.f);
    }

    // Clic gauche → traçage d'un nouveau rayon
    if (inView && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        int px = (int)(mouse.x - origin.x);
        int py = (int)(mouse.y - origin.y);
        traceFromPixel(px, py, (int)sz.x, (int)sz.y);
    }

    if (!m_HasPath) return;

    auto vp = Viz::getViewProj(*m_Scene, sz.x / sz.y);

    // Couleurs par profondeur de rebond
    static const ImU32 SEG_COLORS[] = {
        IM_COL32(255, 255,  90, 230), // rayon primaire : jaune
        IM_COL32(255, 160,  40, 210), // rebond 1 : orange
        IM_COL32(255,  70,  70, 190), // rebond 2 : rouge
        IM_COL32(200,  70, 255, 170), // rebond 3 : violet
        IM_COL32( 70, 200, 255, 150), // rebond 4 : cyan
        IM_COL32(130, 255, 130, 130), // rebond 5+ : vert
    };
    constexpr int N_COLORS = (int)(sizeof(SEG_COLORS) / sizeof(SEG_COLORS[0]));

    for (const auto& seg : m_Path)
    {
        ImU32  col  = SEG_COLORS[std::min(seg.bounce, N_COLORS - 1)];
        ImVec2 a    = Viz::project(seg.from, vp, origin, sz);
        ImVec2 b    = Viz::project(seg.to,   vp, origin, sz);
        bool   aOk  = a.x > -9000.f;
        bool   bOk  = b.x > -9000.f;

        // Segment principal
        if (aOk && bOk)
            dl->AddLine(a, b, col, 2.f);

        // Point d'origine (début du segment)
        if (aOk && m_ShowDots && seg.bounce == 0)
            dl->AddCircleFilled(a, 5.f, IM_COL32(255, 255, 255, 200));

        // Point d'impact
        if (bOk && m_ShowDots && seg.hasHit)
            dl->AddCircleFilled(b, 4.f, col);

        // Flèche de normale au point d'impact
        if (bOk && seg.hasHit && m_ShowNormals)
        {
            glm::vec3 normalEnd = seg.to + seg.normalAtTo * 0.25f;
            ImVec2    c         = Viz::project(normalEnd, vp, origin, sz);
            if (c.x > -9000.f)
                dl->AddLine(b, c, IM_COL32(180, 210, 255, 160), 1.f);
        }
    }
}

} // namespace FufuLab
