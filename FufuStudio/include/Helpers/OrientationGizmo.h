#pragma once

#include "EditorState.h"
#include <Renderer/Renderer.h>
#include <Application/CameraController.h>
#include <glm/glm.hpp>

namespace FufuStudio
{
	// Blender-style orientation compass, in the top-right of the viewport.
	// Deliberately NOT an IEditorTool: it is not an exclusive mode like
	// Transform/Model — it must remain visible regardless of the active tool,
	// so ViewportPanel draws it unconditionally, in addition to the current tool.
	//
	// Clicking an axis (or Numpad 1/3/7, Ctrl+ for the opposite view) snaps
	// the camera to the corresponding view, orbiting around the selected entity
	// (or the origin if nothing is selected), at the camera's current distance.
	class OrientationGizmo
	{
	public:
		explicit OrientationGizmo(Fufu::Renderer& renderer) : m_Renderer(renderer) {}

		void render(EditorState& state, Fufu::CameraController& cam);
		void handleShortcuts(EditorState& state, Fufu::CameraController& cam);

	private:
		void snapTo(EditorState& state, Fufu::CameraController& cam, const glm::vec3& axisDir);

		Fufu::Renderer& m_Renderer;
	};
}
