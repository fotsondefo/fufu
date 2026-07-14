#pragma once

#include <imgui.h>
#include <Project/Scene/Scene.h>

namespace FufuLab
{
    // Interface à implémenter pour chaque papier de recherche.
    // Une démo = une implémentation de ce contrat.
    class IDemo
    {
    public:
        virtual ~IDemo() = default;

        // Nom court affiché dans la liste de sélection (ex : "ReSTIR DI")
        virtual const char* getName() const = 0;

        // Référence du papier (auteurs, conférence, année) — affiché dans l'UI
        virtual const char* getReference() const { return ""; }

        // Appelé quand la démo devient active : setup de la scène, shaders, état
        virtual void onAttach(Fufu::Scene& scene) { (void)scene; }

        // Appelé quand la démo est désactivée (changement de démo ou fermeture)
        virtual void onDetach() {}

        // Logique par frame (mise à jour de l'état algorithmique, animation...)
        virtual void onUpdate(float deltaTime) { (void)deltaTime; }

        // Panneau de paramètres ImGui propre à cette démo (sliders, boutons, stats)
        // Appelé dans le panel "Parameters" de LabLayer — pas besoin de Begin/End.
        virtual void onParametersPanel() {}

        // Dessin d'overlays par-dessus le viewport (bounding boxes, vecteurs, textes)
        // drawList est celui du viewport ImGui.
        virtual void onViewportOverlay(ImDrawList* /*drawList*/, ImVec2 /*origin*/, ImVec2 /*size*/) {}
    };
}
