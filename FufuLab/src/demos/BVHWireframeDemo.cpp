#include "BVHWireframeDemo.h"
#include "VisualizationHelpers.h"
#include "Demo/DemoRegistry.h"
#include <Application/Application.h>
#include <Renderer/Renderer.h>
#include <imgui.h>
#include <stack>
#include <algorithm>

FUFULAB_REGISTER_DEMO("Rendering", "BVH Wireframe", FufuLab::BVHWireframeDemo);

namespace FufuLab
{

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

void BVHWireframeDemo::onAttach(Fufu::Scene& scene)
{
    m_Scene = &scene;
    m_CachedDepth = -999; // force rebuild au premier onViewportOverlay

    auto& gpu = Fufu::Application::get().getRenderer().getGPUScene();
    const auto& tlas = gpu.getTLASNodes();
    const auto& blas = gpu.getBLASNodes();
    const auto& inst = gpu.getInstances();

    m_MaxTLASDepth = tlas.empty() ? 0 : computeMaxDepth(tlas, 0, 0, 0);

    m_MaxBLASDepth = 0;
    for (const auto& i : inst)
    {
        if (i.blasNodeOffset >= 0 && i.blasNodeOffset < (int)blas.size())
            m_MaxBLASDepth = std::max(m_MaxBLASDepth,
                computeMaxDepth(blas, i.blasNodeOffset, 0, 0));
    }

    m_Depth = std::min(m_Depth, std::max(m_MaxTLASDepth, m_MaxBLASDepth));
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

int BVHWireframeDemo::computeMaxDepth(const std::vector<Fufu::GPUBVHNode>& nodes,
                                       int nodeOffset, int relIdx, int depth) const
{
    int absIdx = nodeOffset + relIdx;
    if (absIdx < 0 || absIdx >= (int)nodes.size()) return depth;
    const auto& node = nodes[absIdx];
    if (node.triCount > 0) return depth; // feuille
    int d = depth;
    if (node.left  >= 0) d = std::max(d, computeMaxDepth(nodes, nodeOffset, node.left,  depth + 1));
    if (node.right >= 0) d = std::max(d, computeMaxDepth(nodes, nodeOffset, node.right, depth + 1));
    return d;
}

void BVHWireframeDemo::rebuildCache()
{
    m_Cache.clear();
    m_NodeCountAtDepth = 0;

    auto& gpu  = Fufu::Application::get().getRenderer().getGPUScene();
    const auto& tlas = gpu.getTLASNodes();
    const auto& blas = gpu.getBLASNodes();
    const auto& inst = gpu.getInstances();

    // --- TLAS (espace monde, pas de transform) ---------------------------
    if (m_ShowTLAS && !tlas.empty())
    {
        struct Frame { int idx; int depth; };
        std::stack<Frame> stk;
        stk.push({ 0, 0 });

        while (!stk.empty())
        {
            auto [idx, depth] = stk.top(); stk.pop();
            if (idx < 0 || idx >= (int)tlas.size()) continue;
            const auto& node = tlas[idx];

            if (depth == m_Depth || node.triCount > 0)
            {
                m_Cache.push_back({ glm::vec3(node.boundsMin), glm::vec3(node.boundsMax),
                                    glm::mat4(1.f), false });
                m_NodeCountAtDepth++;
                continue;
            }
            if (node.left  >= 0) stk.push({ node.left,  depth + 1 });
            if (node.right >= 0) stk.push({ node.right, depth + 1 });
        }
    }

    // --- BLAS (espace local, transform de chaque instance) ---------------
    if (m_ShowBLAS && !blas.empty())
    {
        for (const auto& instance : inst)
        {
            int nodeOffset = instance.blasNodeOffset;
            if (nodeOffset < 0 || nodeOffset >= (int)blas.size()) continue;

            struct Frame { int relIdx; int depth; };
            std::stack<Frame> stk;
            stk.push({ 0, 0 });

            while (!stk.empty())
            {
                auto [relIdx, depth] = stk.top(); stk.pop();
                int absIdx = nodeOffset + relIdx;
                if (absIdx < 0 || absIdx >= (int)blas.size()) continue;
                const auto& node = blas[absIdx];

                if (depth == m_Depth || node.triCount > 0)
                {
                    m_Cache.push_back({ glm::vec3(node.boundsMin), glm::vec3(node.boundsMax),
                                        instance.transform, true });
                    m_NodeCountAtDepth++;
                    continue;
                }
                if (node.left  >= 0) stk.push({ node.left,  depth + 1 });
                if (node.right >= 0) stk.push({ node.right, depth + 1 });
            }
        }
    }

    m_CachedDepth = m_Depth;
}

// -----------------------------------------------------------------------
// UI
// -----------------------------------------------------------------------

void BVHWireframeDemo::onParametersPanel()
{
    auto& gpu = Fufu::Application::get().getRenderer().getGPUScene();
    bool changed = false;

    ImGui::TextDisabled("Affichage");
    ImGui::Spacing();

    if (ImGui::Checkbox("TLAS  (espace monde)", &m_ShowTLAS)) changed = true;
    ImGui::SameLine(0.f, 12.f);
    ImGui::TextDisabled("max %d", m_MaxTLASDepth);

    if (ImGui::Checkbox("BLAS  (par instance)", &m_ShowBLAS)) changed = true;
    ImGui::SameLine(0.f, 12.f);
    ImGui::TextDisabled("max %d", m_MaxBLASDepth);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    int maxDepth = std::max(m_MaxTLASDepth, m_MaxBLASDepth);
    if (ImGui::SliderInt("Profondeur", &m_Depth, 0, std::max(maxDepth, 0)))
        changed = true;

    ImGui::SliderFloat("Opacité",   &m_Opacity,   0.f, 1.f);
    ImGui::SliderFloat("Épaisseur", &m_LineThick, 0.5f, 4.f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Statistiques");
    ImGui::Spacing();
    ImGui::Text("Nœuds à la profondeur %d : %d", m_Depth, m_NodeCountAtDepth);
    ImGui::Text("Instances              : %d", (int)gpu.getInstances().size());
    ImGui::Text("Nœuds TLAS             : %d", (int)gpu.getTLASNodes().size());
    ImGui::Text("Nœuds BLAS (total)     : %d", (int)gpu.getBLASNodes().size());
    ImGui::Text("Triangles uniques      : %d", gpu.getTriangleCount());

    if (changed) m_CachedDepth = -999;
}

// -----------------------------------------------------------------------
// Viewport overlay
// -----------------------------------------------------------------------

void BVHWireframeDemo::onViewportOverlay(ImDrawList* dl, ImVec2 origin, ImVec2 sz)
{
    if (!m_Scene || sz.x <= 0.f || sz.y <= 0.f) return;

    if (m_CachedDepth != m_Depth) rebuildCache();

    auto vp   = Viz::getViewProj(*m_Scene, sz.x / sz.y);
    int maxD  = std::max(m_MaxTLASDepth, m_MaxBLASDepth);
    auto alpha = (uint8_t)(m_Opacity * 255.f);

    for (const auto& box : m_Cache)
    {
        ImU32 color = Viz::depthColor(m_Depth, maxD, alpha);
        if (box.isBLAS)
            Viz::drawAABBLocal(dl, box.mn, box.mx, box.modelMat, vp, origin, sz, color, m_LineThick);
        else
            Viz::drawAABB(dl, box.mn, box.mx, vp, origin, sz, color, m_LineThick);
    }
}

} // namespace FufuLab
