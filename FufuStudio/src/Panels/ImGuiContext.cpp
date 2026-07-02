#include "Panels/ImGuiContext.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include "Helpers/FontIcons.h"


namespace FufuStudio 
{

	void ImGuiContext::init(void* nativeWindowHandle, const std::filesystem::path& configDir)
	{
		FUFU_ASSERT(!m_Initialized, "ImGuiContext already initialized");

		// Create the config dir if it doesn't exist
		std::filesystem::create_directories(configDir);
		m_LayoutPath = configDir / "layout.ini";
		m_FontsDir = configDir / "fonts";

		// Init ImGui
		IMGUI_CHECKVERSION();
		::ImGui::CreateContext();

		ImGuiIO& io = ::ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		// Par défaut, ImGui autorise à déplacer une fenêtre en cliquant-glissant
		// n'importe où dans son corps (pas seulement la barre de titre), tant que
		// le clic ne tombe pas sur un widget interactif. Ça entre en conflit avec
		// les interactions du Viewport (gizmo, caméra) et de tous les autres
		// panneaux : on restreint le déplacement à la barre de titre uniquement.
		io.ConfigWindowsMoveFromTitleBarOnly = true;

		// Point to the custom layout file
		m_LayoutPathStr = m_LayoutPath.string();
		io.IniFilename = m_LayoutPathStr.c_str();

		// Backends
		ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(nativeWindowHandle), true);
		ImGui_ImplOpenGL3_Init("#version 430");

		// Style
		initImGuiStyle();

		// Fonts
		loadFonts(m_FontsDir);

		// Load the layout if it exist, Otherwise apply the default one.
		if (!hasLayoutFile())
			applyDefaultLayout();

		m_Initialized = true;
		FUFU_INFO("ImGuiContext initialized — layout: '{}'", m_LayoutPath.string());
	}

	void ImGuiContext::shutdown()
	{
		if (!m_Initialized) 
			return;
		
		saveLayout();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		::ImGui::DestroyContext();
		
		m_Initialized = false;
	}

	void ImGuiContext::beginFrame()
	{
		glClearColor(0.13f, 0.14f, 0.15f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		::ImGui::NewFrame();
	}

	void ImGuiContext::endFrame()
	{
		::ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(::ImGui::GetDrawData());
	}

	bool ImGuiContext::hasLayoutFile() const
	{
		return std::filesystem::exists(m_LayoutPath);
	}

	void ImGuiContext::saveLayout()
	{
		::ImGui::SaveIniSettingsToDisk(m_LayoutPathStr.c_str());
		FUFU_TRACE("Layout saved to '{}'", m_LayoutPath.string());
	}

	void ImGuiContext::loadLayout()
	{
		if (!hasLayoutFile())
		{
			FUFU_WARN("Layout file not found, applying default");
			applyDefaultLayout();
			return;
		}

		::ImGui::LoadIniSettingsFromDisk(m_LayoutPathStr.c_str());
		FUFU_INFO("Layout loaded from '{}'", m_LayoutPath.string());
	}

	void ImGuiContext::resetLayoutToDefault()
	{
		if (hasLayoutFile())
			std::filesystem::remove(m_LayoutPath);
		applyDefaultLayout();
		FUFU_INFO("Layout reset to default");
	}

	void ImGuiContext::applyDefaultLayout()
	{
		// Toutes les fenêtres commencent à Y >= 40 (TitleBar::k_Height) : la barre
		// de titre custom de l'appli occupe les 40 premiers pixels en haut de
		// l'écran et est dessinée par-dessus tout le reste — un panneau qui
		// chevauche cette bande s'y retrouve partiellement caché.
		static const char* k_DefaultLayout = R"(
[Window][MainWindow]
Pos=0,0
Size=1280,720
Collapsed=0

[Window][Viewport]
Pos=0,40
Size=860,680
Collapsed=0

[Window][Hierarchy]
Pos=862,40
Size=418,340
Collapsed=0

[Window][Inspector]
Pos=862,382
Size=418,220
Collapsed=0

[Window][Renderer Settings]
Pos=862,604
Size=418,116
Collapsed=0
)";

		::ImGui::LoadIniSettingsFromMemory(k_DefaultLayout);
		FUFU_INFO("Default layout applied");
	}

	void ImGuiContext::initImGuiStyle()
	{
		::ImGui::StyleColorsDark();
		ImGuiStyle& style = ::ImGui::GetStyle();

		// Window / FRame shape
		style.WindowRounding = 6.f;
		style.WindowBorderSize = 1.f;
		style.WindowPadding = ImVec2(8.f, 8.f);
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_None;
		style.ChildRounding = 4.f;
		style.FrameRounding = 4.f;
		style.GrabRounding = 4.f;
		style.PopupRounding = 4.f;
		style.ScrollbarRounding = 6.f;
		style.TabRounding = 4.f;
		style.FrameBorderSize = 0.f;
		style.ItemSpacing = ImVec2(8.f, 6.f);
		style.FramePadding = ImVec2(5.f, 5.f);
		style.IndentSpacing = 18.f;

		// Color palette VSCode Style : AI generated.
		ImVec4* c = style.Colors;
		c[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.f);
		c[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.f);
		c[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.12f, 0.13f, 0.97f);
		c[ImGuiCol_Border] = ImVec4(0.25f, 0.26f, 0.27f, 1.f);
		c[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.19f, 0.20f, 1.f);
		c[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.25f, 0.27f, 1.f);
		c[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.30f, 0.33f, 1.f);
		c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.f);
		c[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.10f, 0.11f, 1.f);
		c[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.f);
		c[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.f);
		c[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.31f, 0.33f, 1.f);
		c[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.f);
		c[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 0.90f);
		c[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.f);
		c[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.24f, 1.f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.85f);
		c[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.f);
		c[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.24f, 1.f);
		c[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
		c[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.f);
		c[ImGuiCol_Separator] = ImVec4(0.25f, 0.26f, 0.27f, 1.f);
		c[ImGuiCol_Tab] = ImVec4(0.13f, 0.14f, 0.15f, 1.f);
		c[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		c[ImGuiCol_TabActive] = ImVec4(0.18f, 0.35f, 0.58f, 1.f);
		c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.26f, 0.40f, 1.f);
		c[ImGuiCol_Text] = ImVec4(0.90f, 0.91f, 0.92f, 1.f);
		c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.46f, 0.48f, 1.f);
	}

	void ImGuiContext::loadFonts(const std::filesystem::path& fontsDir)
	{
		ImGuiIO& io = ::ImGui::GetIO();
		io.Fonts->Clear();

		const float sizePx = 15.f;
		const float sizeIconsPx = 13.f;

		static const ImWchar k_FA_Ranges[] = { FA_MIN_CODEPOINT, FA_MAX_CODEPOINT, 0 };
		static const ImWchar k_CI_Ranges[] = { CODICON_MIN_CODEPOINT, CODICON_MAX_CODEPOINT, 0 };

		std::filesystem::path notoPath = fontsDir / "NotoSans-Regular.ttf";
		std::filesystem::path notoBoldPath = fontsDir / "NotoSans-Bold.ttf";
		std::filesystem::path notoMonoPath = fontsDir / "NotoSansMono-Regular.ttf";
		std::filesystem::path faPath = fontsDir / "fa-solid-900.ttf";
		std::filesystem::path ciPath = fontsDir / "codicon.ttf";

		// ---- NotoSans Regular ----
		ImFontConfig regularConfig;
		regularConfig.OversampleH = 2;
		regularConfig.OversampleV = 2;

		if (std::filesystem::exists(notoPath))
			m_Fonts.regular = io.Fonts->AddFontFromFileTTF(
				notoPath.string().c_str(), sizePx, &regularConfig);
		else
			m_Fonts.regular = io.Fonts->AddFontDefault();

		// ---- Merge FA ----
		if (std::filesystem::exists(faPath))
		{
			ImFontConfig mergeConfig;
			mergeConfig.MergeMode = true;
			mergeConfig.GlyphMinAdvanceX = sizeIconsPx;
			mergeConfig.OversampleH = 1;
			mergeConfig.OversampleV = 1;
			io.Fonts->AddFontFromFileTTF(
				faPath.string().c_str(), sizeIconsPx, &mergeConfig, k_FA_Ranges);
		}

		// ---- Merge Codicon ----
		if (std::filesystem::exists(ciPath))
		{
			ImFontConfig mergeConfig;
			mergeConfig.MergeMode = true;
			mergeConfig.GlyphMinAdvanceX = sizeIconsPx;
			mergeConfig.OversampleH = 1;
			mergeConfig.OversampleV = 1;
			io.Fonts->AddFontFromFileTTF(
				ciPath.string().c_str(), sizeIconsPx, &mergeConfig, k_CI_Ranges);
		}

		// ---- NotoSans Bold ----
		ImFontConfig boldConfig;
		boldConfig.OversampleH = 2;
		boldConfig.OversampleV = 2;

		if (std::filesystem::exists(notoBoldPath))
			m_Fonts.bold = io.Fonts->AddFontFromFileTTF(notoBoldPath.string().c_str(), sizePx, &boldConfig);
		else
			m_Fonts.bold = m_Fonts.regular;

		// ---- NotoSans Mono ----
		ImFontConfig monoConfig;
		monoConfig.OversampleH = 2;
		monoConfig.OversampleV = 2;

		if (std::filesystem::exists(notoMonoPath))
			m_Fonts.monospace = io.Fonts->AddFontFromFileTTF(notoMonoPath.string().c_str(), sizePx - 1.f, &monoConfig);
		else
			m_Fonts.monospace = m_Fonts.regular;

		// ---- FA standalone ----
		if (std::filesystem::exists(faPath))
			m_Fonts.icons = io.Fonts->AddFontFromFileTTF(faPath.string().c_str(), sizeIconsPx, nullptr, k_FA_Ranges);
		else
			m_Fonts.icons = m_Fonts.regular;

		// ---- Codicon standalone ----
		if (std::filesystem::exists(ciPath))
			m_Fonts.codicons = io.Fonts->AddFontFromFileTTF(ciPath.string().c_str(), sizeIconsPx, nullptr, k_CI_Ranges);
		else
			m_Fonts.codicons = m_Fonts.regular;

		io.Fonts->Build();
		io.FontDefault = m_Fonts.regular;

		FUFU_INFO("Fonts loaded - Regular: {} glyphs", m_Fonts.regular ? m_Fonts.regular->Glyphs.Size : 0);
	}

}