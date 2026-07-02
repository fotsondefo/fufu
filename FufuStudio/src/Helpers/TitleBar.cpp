#include "Helpers/TitleBar.h"
#include "SceneIO.h"
#include <Application/Application.h>
#include "Helpers/FontIcons.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <glad/glad.h>
#include <stb_image.h>
#include <windows.h>

namespace FufuStudio 
{
	bool TitleBar::s_HoveringDragArea = false;
	ImVec2 TitleBar::s_DragP0Screen = ImVec2(0, 0);
	ImVec2 TitleBar::s_DragP1Screen = ImVec2(0, 0);

	static WNDPROC s_OriginalWndProc = nullptr;

	static LRESULT CALLBACK TitleBarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_SIZE || msg == WM_SIZING || msg == WM_EXITSIZEMOVE)
		{
			GLFWwindow* window = reinterpret_cast<GLFWwindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

			if (window && Fufu::Application::get().getWindow().getNativeWindow())
			{
				// Force clear while resizing
				glClearColor(0.13f, 0.14f, 0.15f, 1.f);
				glClear(GL_COLOR_BUFFER_BIT);
				glfwSwapBuffers(window);
			}
		}
		return CallWindowProc(s_OriginalWndProc, hwnd, msg, wParam, lParam);
	}

	void TitleBar::init(const std::filesystem::path& logoPath)
	{	
		// Logo
		if (std::filesystem::exists(logoPath))
		{
			int channels;
			unsigned char* data = stbi_load(logoPath.string().c_str(), &m_LogoWidth, &m_LogoHeight, &channels, 4);

			if (data)
			{
				glGenTextures(1, &m_LogoTextureID);
				glBindTexture(GL_TEXTURE_2D, m_LogoTextureID);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_LogoWidth, m_LogoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				
				stbi_image_free(data);

				FUFU_INFO("TitleBar: logo loaded ({}x{})", m_LogoWidth, m_LogoHeight);
			}
		}
		else
		{
			FUFU_WARN("TitleBar: logo not found at '{}'", logoPath.string());
		}
	}

	void TitleBar::shutdown()
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Fufu::Application::get().getWindow().getNativeWindow());
		HWND hwnd = glfwGetWin32Window(window);

		if (s_OriginalWndProc)
		{
			SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_OriginalWndProc));
		}

		if (m_LogoTextureID)
		{
			glDeleteTextures(1, &m_LogoTextureID);
			m_LogoTextureID = 0;
		}
	}

	void TitleBar::onImGuiRender(EditorState& state)
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(vp->Pos);
		ImGui::SetNextWindowSize(ImVec2(vp->Size.x, k_Height));

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoSavedSettings;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg,
			ImVec4(0.09f, 0.10f, 0.11f, 1.f));

		ImGui::Begin("##TitleBar", nullptr, flags);

		bool titleBarHovered = ImGui::IsWindowHovered();

		ImVec2 p0 = ImGui::GetWindowPos();
		ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowSize().x, p0.y + ImGui::GetWindowSize().y);
		ImVec2 p0Screen = ImVec2(p0.x - vp->Pos.x, p0.y - vp->Pos.y);
		ImVec2 p1Screen = ImVec2(p1.x - vp->Pos.x, p1.y - vp->Pos.y);

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);

		// --- TitleBar Content (logo, menus, buttons) ---
		drawLogo();
		ImGui::SameLine(0.f, 8.f);
		drawMenuBar(state);
		drawProjectName(state);
		drawWindowControls();

		ImGui::End();

		bool hoveringDragArea = titleBarHovered && !ImGui::IsAnyItemHovered();

		s_HoveringDragArea = hoveringDragArea;
		s_DragP0Screen = p0Screen;
		s_DragP1Screen = p1Screen;

		GLFWwindow* window = static_cast<GLFWwindow*>(Fufu::Application::get().getWindow().getNativeWindow());

		glfwSetTitlebarHitTestCallback(window,
			[](GLFWwindow* /*w*/, int x, int y, int* hit)
			{
				if (TitleBar::s_HoveringDragArea &&
					x >= TitleBar::s_DragP0Screen.x &&
					x <= TitleBar::s_DragP1Screen.x &&
					y >= TitleBar::s_DragP0Screen.y &&
					y <= TitleBar::s_DragP1Screen.y)
				{
					*hit = 1;
				}
				else
				{
					*hit = 0;
				}
			}
		);
	}

	void TitleBar::drawLogo()
	{
		if (m_LogoTextureID)
		{
			float logoH = 24.f;
			float logoW = logoH * (static_cast<float>(m_LogoWidth) / static_cast<float>(m_LogoHeight));
			ImGui::Image(
				reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_LogoTextureID)),
				ImVec2(logoW, logoH)
			);
		}
		else
		{
			// If there's no logo 
			ImGui::Text("FE");
		}
	}

	void TitleBar::drawMenuBar(EditorState& state)
	{
		ImGui::SetCursorPosY((k_Height - ImGui::GetFrameHeight()) * 0.5f);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.08f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.14f));

		auto menuItem = [](const char* label) -> bool {
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));
			
			bool clicked = ImGui::Button(label);
			
			ImGui::PopStyleVar();
			
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
				ImGui::OpenPopup(label);
			
			return clicked;
		};

		// File
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));
		
		if (ImGui::Button("File"))
			ImGui::OpenPopup("##FileMenu");

		ImGui::PopStyleVar();

		if (ImGui::BeginPopup("##FileMenu"))
		{
			if (ImGui::MenuItem(ICON_FA_FILE " New Scene", "Ctrl+N"))
				SceneIO::newScene(state);
			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Scene...", "Ctrl+O"))
				SceneIO::openScene(state);
			
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_FLOPPY_O " Save", "Ctrl+S"))
				SceneIO::saveScene(state);
			if (ImGui::MenuItem(ICON_FA_FLOPPY_O " Save As...", "Ctrl+Shift+S"))
				SceneIO::saveSceneAs(state);
			
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_CARET_SQUARE_O_LEFT " Exit"))
				Fufu::Application::get().close();
		
			ImGui::EndPopup();
		}

		// Edit
		ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));
		if (ImGui::Button("Edit"))
			ImGui::OpenPopup("##EditMenu");
		ImGui::PopStyleVar();

		if (ImGui::BeginPopup("##EditMenu"))
		{
			ImGui::TextDisabled("(coming soon)");
			ImGui::EndPopup();
		}

		// View
		ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));
		
		if (ImGui::Button("View"))
			ImGui::OpenPopup("##ViewMenu");
		
		ImGui::PopStyleVar();

		if (ImGui::BeginPopup("##ViewMenu"))
		{
			if (ImGui::MenuItem(ICON_FA_COLUMNS " Reset Layout"))
			{
				if (state.imGuiContext)
					state.imGuiContext->resetLayoutToDefault();
			}

			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}

	void TitleBar::drawProjectName(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();

		std::string title;
		if (pm.hasProject())
		{
			title = pm.getCurrentProject().getName();
			auto scene = state.getActiveScene();
			if (scene)
				title += "  -  " + scene->getName();
		}
		else
		{
			title = "Fufu Engine";
		}

		float textW = ImGui::CalcTextSize(title.c_str()).x;
		float windowW = ImGui::GetWindowWidth();
		float centeredX = (windowW - textW) * 0.5f;

		float cursorX = ImGui::GetCursorPosX();
		if (centeredX < cursorX + 20.f)
			centeredX = cursorX + 20.f;

		ImGui::SameLine();
		ImGui::SetCursorPosX(centeredX);
		ImGui::SetCursorPosY((k_Height - ImGui::GetTextLineHeight()) * 0.5f);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.71f, 0.72f, 1.f));
		ImGui::Text("%s", title.c_str());
		ImGui::PopStyleColor();
	}

	void TitleBar::drawWindowControls()
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(
			Fufu::Application::get().getWindow().getNativeWindow());

		float windowW = ImGui::GetWindowWidth();
		float btnW = 46.f;
		float btnH = k_Height;
		float startX = windowW - btnW * 3.f;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

		// Minimize
		ImGui::SetCursorPos(ImVec2(startX, 0.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button("##minimize", ImVec2(btnW, btnH)))
			glfwIconifyWindow(window);

		// Icons
		ImVec2 minBtnMin = ImGui::GetItemRectMin();
		ImVec2 minBtnMax = ImGui::GetItemRectMax();
		ImVec2 center = ImVec2((minBtnMin.x + minBtnMax.x) * 0.5f, (minBtnMin.y + minBtnMax.y) * 0.5f);
		ImGui::GetWindowDrawList()->AddLine(ImVec2(center.x - 6.f, center.y + 1.f), ImVec2(center.x + 6.f, center.y + 1.f), IM_COL32(200, 200, 200, 255), 1.5f);

		ImGui::PopStyleColor(3);

		// Maximize / Restore
		ImGui::SetCursorPos(ImVec2(startX + btnW, 0.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button("##maximize", ImVec2(btnW, btnH)))
		{
			if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED))
				glfwRestoreWindow(window);
			else
				glfwMaximizeWindow(window);
		}

		// Icone maximize
		ImVec2 maxBtnMin = ImGui::GetItemRectMin();
		ImVec2 maxBtnMax = ImGui::GetItemRectMax();
		ImVec2 maxCenter = ImVec2((maxBtnMin.x + maxBtnMax.x) * 0.5f, (maxBtnMin.y + maxBtnMax.y) * 0.5f);

		bool isMaximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

		if (isMaximized)
		{
			// Restor icon
			ImGui::GetWindowDrawList()->AddRect(ImVec2(maxCenter.x - 4.f, maxCenter.y - 6.f), ImVec2(maxCenter.x + 6.f, maxCenter.y + 4.f), IM_COL32(200, 200, 200, 255), 0.f, 0, 1.5f);
			ImGui::GetWindowDrawList()->AddRect(ImVec2(maxCenter.x - 6.f, maxCenter.y - 4.f), ImVec2(maxCenter.x + 4.f, maxCenter.y + 6.f), IM_COL32(200, 200, 200, 255), 0.f, 0, 1.5f);
		}
		else
		{
			// Maximize icon
			ImGui::GetWindowDrawList()->AddRect(ImVec2(maxCenter.x - 6.f, maxCenter.y - 6.f), ImVec2(maxCenter.x + 6.f, maxCenter.y + 6.f), IM_COL32(200, 200, 200, 255), 0.f, 0, 1.5f);
		}

		ImGui::PopStyleColor(3);

		// Close
		ImGui::SetCursorPos(ImVec2(startX + btnW * 2.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.18f, 0.18f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.10f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button("##close", ImVec2(btnW, btnH)))
			Fufu::Application::get().close();

		// Clos icon
		ImVec2 closeBtnMin = ImGui::GetItemRectMin();
		ImVec2 closeBtnMax = ImGui::GetItemRectMax();
		ImVec2 closeCenter = ImVec2(
			(closeBtnMin.x + closeBtnMax.x) * 0.5f,
			(closeBtnMin.y + closeBtnMax.y) * 0.5f
		);
		
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(closeCenter.x - 6.f, closeCenter.y - 6.f),
			ImVec2(closeCenter.x + 6.f, closeCenter.y + 6.f),
			IM_COL32(200, 200, 200, 255), 1.5f
		);
		
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(closeCenter.x + 6.f, closeCenter.y - 6.f),
			ImVec2(closeCenter.x - 6.f, closeCenter.y + 6.f),
			IM_COL32(200, 200, 200, 255), 1.5f
		);

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(3);
	}

}