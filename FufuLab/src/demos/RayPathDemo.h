#pragma once
#include "Demo/IDemo.h"
#include <Renderer/BVH.h>
#include <Renderer/GPUBuffers.h>
#include <vector>

namespace FufuLab
{

// Visualise le chemin d'un rayon dans la scène.
// Cliquer dans le viewport trace un rayon depuis la caméra vers ce pixel
// et affiche ses segments de rebond sous forme de lignes colorées.
//
// Les rebonds sont des réflexions miroir (reflect sur la normale géométrique)
// — ils ne correspondent pas à ce que le path tracer calcule (qui est
// stochastique) mais illustrent comment un rayon traverse la géométrie.
class RayPathDemo : public IDemo
{
public:
    const char* getName() const override { return "Ray Path"; }
    const char* getReference() const override
    {
        return "Ray Path Visualization\n"
               "Cliquer dans le viewport pour tracer un rayon.\n"
               "Rebonds : réflexion miroir sur la normale géométrique.";
    }

    void onAttach(Fufu::Scene& scene) override { m_Scene = &scene; }
    void onDetach()                   override { m_Scene = nullptr; clearPath(); }
    void onUpdate(float)              override {}
    void onParametersPanel()          override;
    void onViewportOverlay(ImDrawList*, ImVec2, ImVec2) override;

private:
    // Un segment du chemin : from → to (to = hit ou point à l'infini)
    struct Segment
    {
        glm::vec3 from, to;
        glm::vec3 normalAtTo; // normale monde à 'to' (valide si hasHit)
        bool      hasHit;
        int       bounce;     // 0 = rayon primaire
    };

    // Résultat d'un test d'intersection contre la scène entière
    struct HitResult
    {
        bool      hit          = false;
        float     t            = 1e30f;
        glm::vec3 position     = {};
        glm::vec3 normal       = {};
        int       instanceIndex = -1;
        int       triIndex      = -1;
    };

    void clearPath();
    void traceFromPixel(int px, int py, int viewW, int viewH);
    HitResult castRay(glm::vec3 origin, glm::vec3 dir) const;

    // Intersections géométriques (statiques pour éviter de polluer l'état)
    static bool intersectAABB(glm::vec3 ro, glm::vec3 rd,
                               glm::vec3 mn, glm::vec3 mx, float& t);
    static bool intersectTriangle(glm::vec3 ro, glm::vec3 rd,
                                   glm::vec3 v0, glm::vec3 v1, glm::vec3 v2,
                                   float& t);
    static void traverseBLAS(glm::vec3 ro, glm::vec3 rd,
                              int nodeOffset, int triOffset,
                              const std::vector<Fufu::GPUBVHNode>&          nodes,
                              const std::vector<Fufu::GPUTrianglePosition>& tris,
                              float& closest, int& hitTri);

    Fufu::Scene* m_Scene = nullptr;

    // Chemin courant
    std::vector<Segment> m_Path;
    bool m_HasPath = false;

    // Paramètres UI
    int  m_MaxBounces  = 5;
    bool m_ShowNormals = true;
    bool m_ShowDots    = true;

    // Infos sur le dernier pixel cliqué
    int m_LastPixel[2] = { -1, -1 };
};

} // namespace FufuLab
