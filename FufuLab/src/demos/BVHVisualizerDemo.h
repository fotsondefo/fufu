#pragma once
#include "Demo/IDemo.h"
#include <Project/Assets/MeshAsset.h>
#include <Renderer/BVH.h>
#include <string>
#include <vector>

namespace FufuLab
{
    // Analyse la structure du BVH construit par SAH et affiche ses statistiques.
    // Utile pour vérifier la qualité de la construction : profondeur, équilibre,
    // taille des feuilles.
    class BVHVisualizerDemo : public IDemo
    {
    public:
        const char* getName()      const override { return "BVH Visualizer"; }
        const char* getReference() const override
        {
            return "Surface Area Heuristic (SAH)\n"
                   "Goldsmith & Salmon 1987\n"
                   "\"Automatic Creation of Object Hierarchies for Ray Tracing\"";
        }

        void onAttach(Fufu::Scene& scene) override;
        void onDetach() override {}
        void onUpdate(float) override {}
        void onParametersPanel() override;

    private:
        struct BVHStats
        {
            int nodeCount    = 0;
            int leafCount    = 0;
            int internalCount = 0;
            int maxDepth     = 0;
            float avgLeafSize = 0.f;
            int   minLeafSize = 0;
            int   maxLeafSize = 0;
            std::vector<int> depthHistogram; // nodeCount par profondeur
            std::vector<int> leafSizeHistogram;
        };

        void analyzeNodes(const std::vector<Fufu::GPUBVHNode>& nodes, int nodeIdx,
                          int depth, BVHStats& stats);
        BVHStats computeStats(const std::vector<Fufu::GPUBVHNode>& nodes);

        Fufu::Scene* m_Scene = nullptr;
        BVHStats m_Stats;
        bool m_HasStats = false;

        std::string m_MeshPath;
        char m_MeshPathBuf[512] = {};
    };
}
