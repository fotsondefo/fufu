#pragma once

namespace FufuLab
{
    // Écran affiché au démarrage de FufuLab tant qu'aucun projet n'est chargé.
    // FufuLab n'est pas un outil de création : il ouvre des projets existants
    // créés dans FufuStudio. Retourne true dès qu'un projet est chargé.
    class LabProjectScreen
    {
    public:
        bool onImGuiRender();

    private:
        void drawRecentProjects();
        bool openProjectDialog();
    };
}
