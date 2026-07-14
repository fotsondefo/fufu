#pragma once

#include "Demo/IDemo.h"
#include <Renderer/Renderer.h>
#include <Project/Scene/Scene.h>

namespace FufuLab
{
    // Scène de référence classique pour évaluer un path tracer.
    // Créée de manière procédurale (pas de fichier mesh), donc toujours
    // disponible même sans projet chargé.
    class CornellBoxDemo : public IDemo
    {
    public:
        const char* getName()      const override { return "Cornell Box"; }
        const char* getReference() const override
        {
            return "Cornell Box — Goral et al. 1984\n"
                   "\"Modeling the Interaction of Light Between Diffuse Surfaces\"";
        }

        void onAttach(Fufu::Scene& scene) override;
        void onDetach() override;
        void onParametersPanel() override;

    private:
        Fufu::Scene*  m_Scene     = nullptr;
        Fufu::Renderer* m_Renderer = nullptr;

        float m_LightIntensity = 10.f;
        float m_Roughness      = 1.f;
    };
}
