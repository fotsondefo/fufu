#include "Panels/ImGuiContext.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#pragma region FontMacros

// Ranges Unicode FontAwesome 6
#define FA_MIN_CODEPOINT 0xe005
#define FA_MAX_CODEPOINT 0xf8ff

// Ranges Unicode Codicon
#define CODICON_MIN_CODEPOINT 0xea60
#define CODICON_MAX_CODEPOINT 0xebeb

// Common macros for icons
#define ICON_FA_FOLDER      "\xef\x81\xbb"
#define ICON_FA_FILE        "\xef\x82\x9b"
#define ICON_FA_SAVE        "\xef\x83\x87"
#define ICON_FA_PLAY        "\xef\x81\x8b"
#define ICON_FA_TRASH       "\xef\x87\xb8"
#define ICON_FA_PLUS        "\xef\x81\xa7"
#define ICON_FA_EYE         "\xef\x81\xae"
#define ICON_FA_CUBE        "\xef\x86\xb2"
#define ICON_FA_CAMERA      "\xef\x80\xb0"
#define ICON_FA_COG         "\xef\x80\x93"

#define ICON_CI_FILE        "\xea\x77"
#define ICON_CI_FOLDER      "\xea\x83"
#define ICON_CI_SETTINGS    "\xeb\x51"

#pragma endregion


namespace FufuStudio 
{

	void ImGuiContext::init(void* nativeWindowHandle, const std::filesystem::path& configDir)
	{
		FUFU_ASSERT(!m_Initialized, "ImGuiContext already initialized");

		// Create the config dir if it doesn't exist
		std::filesystem::create_directories(configDir);
		m_LayoutPath = configDir / "layout.ini";
		m_FontsDir = std::filesystem::current_path() / "assets" / "fonts";

		// Init ImGui
		IMGUI_CHECKVERSION();
		::ImGui::CreateContext();

		ImGuiIO& io = ::ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

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
		static const char* k_DefaultLayout = R"(
[Window][MainWindow]
Pos=0,0
Size=1280,720
Collapsed=0

[Window][Viewport]
Pos=0,20
Size=860,700
Collapsed=0

[Window][Hierarchy]
Pos=862,20
Size=418,340
Collapsed=0

[Window][Inspector]
Pos=862,362
Size=418,220
Collapsed=0

[Window][Renderer Settings]
Pos=862,584
Size=418,136
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
		style.ChildRounding = 4.f;
		style.FrameRounding = 4.f;
		style.GrabRounding = 4.f;
		style.PopupRounding = 4.f;
		style.ScrollbarRounding = 6.f;
		style.TabRounding = 4.f;
		style.WindowBorderSize = 1.f;
		style.FrameBorderSize = 0.f;
		style.ItemSpacing = ImVec2(8.f, 6.f);
		style.FramePadding = ImVec2(6.f, 4.f);
		style.WindowPadding = ImVec2(8.f, 8.f);
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

		// --- Ranges glyphes ---
		// FontAwesome
		static const ImWchar k_FA_Ranges[] = { FA_MIN_CODEPOINT, FA_MAX_CODEPOINT, 0 };
		// Codicon
		static const ImWchar k_CI_Ranges[] = { CODICON_MIN_CODEPOINT, CODICON_MAX_CODEPOINT, 0 };

		ImFontConfig config;
		config.OversampleH = 2;
		config.OversampleV = 2;

		// ---- NotoSans Regular ----
		std::filesystem::path notoPath = fontsDir / "NotoSans-Regular.ttf";
		if (std::filesystem::exists(notoPath))
		{
			m_Fonts.regular = io.Fonts->AddFontFromFileTTF(notoPath.string().c_str(), sizePx, &config);
		}
		else
		{
			FUFU_WARN("Font not found: '{}'", notoPath.string());
			m_Fonts.regular = io.Fonts->AddFontDefault();
		}

		// ---- NotoSans Bold ----
		std::filesystem::path notoBoldPath = fontsDir / "NotoSans-Bold.ttf";
		if (std::filesystem::exists(notoBoldPath))
		{
			m_Fonts.bold = io.Fonts->AddFontFromFileTTF(notoBoldPath.string().c_str(), sizePx, &config);
		}
		else
		{
			m_Fonts.bold = m_Fonts.regular;
		}

		// ---- NotoSans Mono ----
		std::filesystem::path notoMonoPath = fontsDir / "NotoSansMono-Regular.ttf";
		if (std::filesystem::exists(notoMonoPath))
		{
			m_Fonts.monospace = io.Fonts->AddFontFromFileTTF(notoMonoPath.string().c_str(), sizePx - 1.f, &config);
		}
		else
		{
			m_Fonts.monospace = m_Fonts.regular;
		}

		// ---- FontAwesome ----
		std::filesystem::path faPath = fontsDir / "fa-solid-900.ttf";
		if (std::filesystem::exists(faPath))
		{
			ImFontConfig faConfig;
			faConfig.MergeMode = true;
			faConfig.GlyphMinAdvanceX = sizeIconsPx;
			faConfig.OversampleH = 1;
			faConfig.OversampleV = 1;

			// Font for icons
			m_Fonts.icons = io.Fonts->AddFontFromFileTTF(faPath.string().c_str(), sizeIconsPx, nullptr, k_FA_Ranges);

			io.Fonts->AddFontFromFileTTF(faPath.string().c_str(), sizeIconsPx, &faConfig, k_FA_Ranges);
		}
		else
		{
			FUFU_WARN("FontAwesome not found: '{}'", faPath.string());
			m_Fonts.icons = m_Fonts.regular;
		}

		// ---- Codicon ----
		std::filesystem::path ciPath = fontsDir / "codicon.ttf";
		if (std::filesystem::exists(ciPath))
		{
			m_Fonts.codicons = io.Fonts->AddFontFromFileTTF(ciPath.string().c_str(), sizeIconsPx, nullptr, k_CI_Ranges);
		}
		else
		{
			FUFU_WARN("Codicon not found: '{}'", ciPath.string());
			m_Fonts.codicons = m_Fonts.regular;
		}

		io.Fonts->Build();

		// La Regular devient la police par défaut
		io.FontDefault = m_Fonts.regular;

		FUFU_INFO("Fonts loaded from '{}'", fontsDir.string());
	}

}