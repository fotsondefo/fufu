#include <EntryPoint.h>
#include "Layers/LabLayer.h"

class FufuLabApp : public Fufu::Application
{
public:
    explicit FufuLabApp()
        : Fufu::Application(Fufu::WindowProps{ "FufuLab - Research", 1440, 900, true })
    {
        pushLayer(new FufuLab::LabLayer());
    }
};

Fufu::Application* CreateApplication()
{
    return new FufuLabApp();
}
