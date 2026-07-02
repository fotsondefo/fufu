#pragma once

#include "EditorState.h"
#include <Renderer/Renderer.h>
#include <glm/glm.hpp>

namespace FufuStudio
{
	// Boussole d'orientation façon Blender, en haut à droite du viewport.
	// Volontairement PAS un IEditorTool : ce n'est pas un mode exclusif comme
	// Transform/Model — elle doit rester visible quel que soit l'outil actif,
	// donc ViewportPanel la dessine systématiquement, en plus de l'outil courant.
	//
	// Cliquer un axe (ou Pavé Num. 1/3/7, Ctrl+ pour la vue opposée) fait
	// sauter la caméra sur la vue correspondante, en orbite autour de l'entité
	// sélectionnée (ou de l'origine si rien n'est sélectionné), à la distance
	// actuelle de la caméra.
	class OrientationGizmo
	{
	public:
		explicit OrientationGizmo(Fufu::Renderer& renderer) : m_Renderer(renderer) {}

		void render(EditorState& state);
		void handleShortcuts(EditorState& state);

	private:
		void snapTo(EditorState& state, const glm::vec3& axisDir);

		Fufu::Renderer& m_Renderer;
	};
}
