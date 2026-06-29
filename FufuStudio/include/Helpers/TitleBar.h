#pragma once

#include "EditorState.h"

namespace FufuStudio 
{

	class TitleBar
	{
		struct HitTestRect
		{
			float x, y, w, h;
		};

	public:
		static constexpr float k_Height = 40.f;

		void init(const std::filesystem::path& logoPath);
		void shutdown();

		void onImGuiRender(EditorState& state);

		static float getHeight() { return k_Height; }

	private:
		void drawLogo();
		void drawMenuBar(EditorState& state);
		void drawProjectName(EditorState& state);
		void drawWindowControls();
		void registerNonDraggable(float x, float y, float w, float h);

	private:
		uint32_t m_LogoTextureID = 0;
		int m_LogoWidth = 0;
		int m_LogoHeight = 0;
		void* m_OriginalWndProc = nullptr;


		bool m_Dragging = false;
		glm::vec2 m_DragStartPos = glm::vec2(0.f);
		glm::vec2 m_WindowStartPos = glm::vec2(0.f);

		std::vector<HitTestRect> m_NonDraggableRects;
	};

}