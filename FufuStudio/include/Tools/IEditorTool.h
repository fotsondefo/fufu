#pragma once

namespace FufuStudio
{
	struct EditorState;

	// Un outil actif dans le Viewport (Transform aujourd'hui, Sculpt/Groom plus
	// tard). ViewportPanel ne conna�t que cette interface : ajouter un nouvel
	// outil ne touche jamais ViewportPanel, seulement l� o� il s'enregistre
	// dans le ToolManager.
	class IEditorTool
	{
	public:
		virtual ~IEditorTool() = default;

		// Boutons rapides affich�s dans la barre d'outils du viewport
		// (ex : Translate/Rotate/Scale pour l'outil Transform).
		virtual void onToolbar(EditorState& state) {}

		// Dessine l'overlay de l'outil par-dessus le rendu (gizmo, brush, etc.).
		// Appel� pendant que la fen�tre ImGui du viewport est ouverte.
		virtual void onViewportOverlay(EditorState& state) {}

		// Raccourcis clavier propres � l'outil (ex : W/E/R, Tab).
		virtual void onShortcuts(EditorState& state) {}

		// Vrai pendant une interaction active (drag en cours) : sert � �viter
		// que d'autres syst�mes n'interf�rent pendant que l'outil est utilis�.
		virtual bool isUsing() const { return false; }

		virtual const char* getName() const = 0;
	};
}
