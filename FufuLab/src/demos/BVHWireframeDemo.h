#pragma once
#include "Demo/IDemo.h"
#include <Renderer/BVH.h>
#include <Renderer/GPUBuffers.h>
#include <vector>

namespace FufuLab
{

// Visualise le BVH de la scène sous forme de fils de fer AABB.
//   - TLAS : nœuds en espace monde (partitionnement global des instances)
//   - BLAS : nœuds en espace local, dessinés dans l'espace monde via
//            la transform de chaque instance
// Le slider "Depth" isole un niveau de l'arbre. Afficher des niveaux
// profonds sur un mesh dense (Dragon, etc.) peut dessiner plusieurs
// milliers de boîtes — réduire l'opacité aide à la lisibilité.
class BVHWireframeDemo : public IDemo
{
public:
    const char* getName() const override { return "BVH Wireframe"; }
    const char* getReference() const override
    {
        return "Goldsmith & Salmon (1987)\n"
               "\"Automatic Creation of Object Hierarchies for Ray Tracing\"\n"
               "Visualisation AABB par niveau de l'arbre BVH (TLAS + BLAS).";
    }

    void onAttach(Fufu::Scene& scene) override;
    void onDetach()                   override { m_Scene = nullptr; }
    void onUpdate(float)              override {}
    void onParametersPanel()          override;
    void onViewportOverlay(ImDrawList*, ImVec2, ImVec2) override;

private:
    int  computeMaxDepth(const std::vector<Fufu::GPUBVHNode>& nodes,
                         int nodeOffset, int relIdx, int depth) const;
    void rebuildCache();

    Fufu::Scene* m_Scene = nullptr;

    // Paramètres UI
    int   m_Depth     = 0;
    bool  m_ShowTLAS  = true;
    bool  m_ShowBLAS  = true;
    float m_Opacity   = 0.75f;
    float m_LineThick = 1.f;

    // Statistiques
    int m_MaxTLASDepth      = 0;
    int m_MaxBLASDepth      = 0;
    int m_NodeCountAtDepth  = 0;

    // Cache des AABB à dessiner (recalculé quand la profondeur change)
    struct CachedAABB
    {
        glm::vec3 mn, mx;
        glm::mat4 modelMat; // identité pour TLAS (espace monde), transform instance pour BLAS
        bool      isBLAS;
    };
    std::vector<CachedAABB> m_Cache;
    int m_CachedDepth = -999;
};

} // namespace FufuLab
