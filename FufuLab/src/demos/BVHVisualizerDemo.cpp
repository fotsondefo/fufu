#include "BVHVisualizerDemo.h"
#include "Demo/DemoRegistry.h"
#include <Application/Application.h>
#include <imgui.h>
#include <algorithm>

FUFULAB_REGISTER_DEMO("Rendering", "BVH Visualizer", FufuLab::BVHVisualizerDemo);

namespace FufuLab
{
    void BVHVisualizerDemo::onAttach(Fufu::Scene& scene)
    {
        m_Scene = &scene;
    }

    void BVHVisualizerDemo::onParametersPanel()
    {
        ImGui::TextDisabled("Enter the path of a mesh already loaded");
        ImGui::TextDisabled("in the current project to analyze its BVH.");
        ImGui::Spacing();

        bool analyze = ImGui::InputText("Mesh path", m_MeshPathBuf, sizeof(m_MeshPathBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        analyze |= ImGui::Button("Analyze");

        if (analyze)
        {
            m_HasStats = false;
            auto& pm = Fufu::Application::get().getProjectManager();
            if (!pm.hasProject())
            {
                ImGui::TextColored(ImVec4(1.f,0.4f,0.4f,1.f), "No project loaded.");
                return;
            }

            auto asset = pm.getCurrentProject().getAssetManager().getMesh(m_MeshPathBuf);
            if (!asset)
            {
                ImGui::TextColored(ImVec4(1.f,0.4f,0.4f,1.f),
                    "Mesh not found. Load it first via FufuStudio.");
                return;
            }

            const auto& blas = asset->getOrBuildBLAS(0);
            m_Stats = computeStats(blas.nodes);
            m_HasStats = true;
        }

        if (!m_HasStats) return;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("BVH Statistics (LOD 0)");
        ImGui::Spacing();

        ImGui::Text("Total nodes    %d", m_Stats.nodeCount);
        ImGui::Text("  Internal     %d", m_Stats.internalCount);
        ImGui::Text("  Leaves       %d", m_Stats.leafCount);
        ImGui::Text("Max depth      %d", m_Stats.maxDepth);
        ImGui::Text("Leaf size      min=%d  avg=%.1f  max=%d",
            m_Stats.minLeafSize, m_Stats.avgLeafSize, m_Stats.maxLeafSize);

        ImGui::Spacing();
        ImGui::TextDisabled("Nodes per depth level");
        if (!m_Stats.depthHistogram.empty())
        {
            std::vector<float> f(m_Stats.depthHistogram.begin(), m_Stats.depthHistogram.end());
            float maxVal = *std::max_element(f.begin(), f.end());
            ImGui::PlotHistogram("##depth", f.data(), (int)f.size(),
                0, nullptr, 0.f, maxVal, ImVec2(-1.f, 80.f));
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Leaf size distribution");
        if (!m_Stats.leafSizeHistogram.empty())
        {
            std::vector<float> f(m_Stats.leafSizeHistogram.begin(), m_Stats.leafSizeHistogram.end());
            float maxVal = *std::max_element(f.begin(), f.end());
            ImGui::PlotHistogram("##leafsz", f.data(), (int)f.size(),
                0, nullptr, 0.f, maxVal, ImVec2(-1.f, 80.f));
        }
    }

    void BVHVisualizerDemo::analyzeNodes(const std::vector<Fufu::GPUBVHNode>& nodes,
                                          int nodeIdx, int depth, BVHStats& stats)
    {
        if (nodeIdx < 0 || nodeIdx >= (int)nodes.size()) return;

        const auto& node = nodes[nodeIdx];
        stats.nodeCount++;
        stats.maxDepth = std::max(stats.maxDepth, depth);

        if ((int)stats.depthHistogram.size() <= depth)
            stats.depthHistogram.resize(depth + 1, 0);
        stats.depthHistogram[depth]++;

        if (node.triCount > 0)
        {
            stats.leafCount++;
            int sz = node.triCount;
            stats.minLeafSize = std::min(stats.minLeafSize, sz);
            stats.maxLeafSize = std::max(stats.maxLeafSize, sz);
            if ((int)stats.leafSizeHistogram.size() <= sz)
                stats.leafSizeHistogram.resize(sz + 1, 0);
            stats.leafSizeHistogram[sz]++;
        }
        else
        {
            stats.internalCount++;
            analyzeNodes(nodes, node.left,  depth + 1, stats);
            analyzeNodes(nodes, node.right, depth + 1, stats);
        }
    }

    BVHVisualizerDemo::BVHStats BVHVisualizerDemo::computeStats(
        const std::vector<Fufu::GPUBVHNode>& nodes)
    {
        BVHStats stats;
        if (nodes.empty()) return stats;

        stats.minLeafSize = INT_MAX;
        analyzeNodes(nodes, 0, 0, stats);

        if (stats.minLeafSize == INT_MAX) stats.minLeafSize = 0;

        long long totalTris = 0;
        for (int i = 0; i < (int)stats.leafSizeHistogram.size(); ++i)
            totalTris += (long long)i * stats.leafSizeHistogram[i];
        stats.avgLeafSize = stats.leafCount > 0
            ? (float)totalTris / (float)stats.leafCount : 0.f;

        return stats;
    }
}
