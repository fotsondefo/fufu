#pragma once

// Chaque application consommatrice (Studio, Runtime) inclut ce header
// et implémente CreateApplication() pour retourner leur sous-classe d'Application.
//
// Exemple dans DelosStudio/src/main.cpp :
//   #include <Delos/EntryPoint.h>
//   Application* CreateApplication() { return new StudioApp(); }

#include "Application.h"

extern Application* CreateApplication();

int main(int argc, char** argv)
{
    auto* app = CreateApplication();
    app->run();
    delete app;
    return 0;
}
