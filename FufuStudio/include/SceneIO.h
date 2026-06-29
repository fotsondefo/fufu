#pragma once

#include "EditorState.h"

namespace FufuStudio 
{

	class SceneIO
	{
	public:
		// Returns true if the operations succeeded
		static bool saveScene(EditorState& state);
		static bool saveSceneAs(EditorState& state);
		static bool openScene(EditorState& state);
		static bool newScene(EditorState& state);
	};

}