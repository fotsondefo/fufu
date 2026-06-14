#pragma once

#include "IEditorPanel.h"

namespace FufuStudio 
{

	class InspectorPanel : public IEditorPanel
	{
	public:
		void onImGuiRender(EditorState& state) override;

	private:
		void drawTag(Fufu::Entity entity);
		void drawTransform(Fufu::Entity entity, EditorState& state);
		void drawMesh(Fufu::Entity entity);
		void drawMaterial(Fufu::Entity entity, EditorState& state);
		void drawCamera(Fufu::Entity entity);

		template<typename T>
		void drawAddComponentButton(Fufu::Entity entity, const char* label);
	};

}