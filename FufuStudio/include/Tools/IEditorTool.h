#pragma once

namespace FufuStudio
{
	struct EditorState;

	// An active tool in the Viewport (Transform today, Sculpt/Groom later).
	// later). ViewportPanel only knows this interface: adding a new
	// tool never touches ViewportPanel, only where it registers
	// in the ToolManager.
	class IEditorTool
	{
	public:
		virtual ~IEditorTool() = default;

		// Quick buttons displayed in the viewport toolbar
		// (e.g. Translate/Rotate/Scale for the Transform tool).
		virtual void onToolbar(EditorState& state) {}

		// Draws the tool overlay on top of the render (gizmo, brush, etc.).
		// Called while the viewport ImGui window is open.
		virtual void onViewportOverlay(EditorState& state) {}

		// Keyboard shortcuts specific to this tool (e.g. W/E/R, Tab).
		virtual void onShortcuts(EditorState& state) {}

		// True during an active interaction (drag in progress): used to avoid
		// other systems from interfering while the tool is in use.
		virtual bool isUsing() const { return false; }

		virtual const char* getName() const = 0;
	};
}
