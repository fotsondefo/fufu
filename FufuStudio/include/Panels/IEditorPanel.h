#pragma once

#include "EditorState.h"

namespace FufuStudio 
{
	class IEditorPanel
	{
	public:
		virtual ~IEditorPanel() = default;
		virtual void onImGuiRender(EditorState& state) = 0;
	};
}