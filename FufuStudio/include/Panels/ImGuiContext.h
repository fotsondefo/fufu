#pragma once

#include <imgui.h>
#include <filesystem>
#include <string>

namespace FufuStudio
{
	struct FontRegistry
	{
		ImFont* regular = nullptr;
		ImFont* bold = nullptr;
		ImFont* monospace = nullptr;
		ImFont* icons = nullptr;
		ImFont* codicons = nullptr;
	};

	class ImGuiContext
	{
	public:
		ImGuiContext() = default;
		~ImGuiContext() = default;

		
		void init(void* nativeWindowHandle, const std::filesystem::path& configDir);
		void shutdown();

		void beginFrame();
		void endFrame();

		const FontRegistry& getFonts() const { return m_Fonts; }

		void saveLayout();
		void loadLayout();
		void resetLayoutToDefault();
		bool hasLayoutFile() const;

	private:
		void initImGuiStyle();
		void loadFonts(const std::filesystem::path& fontsDir);
		void applyDefaultLayout();

		std::string m_LayoutPathStr;
		std::filesystem::path m_LayoutPath;
		std::filesystem::path m_FontsDir;
		FontRegistry m_Fonts;
		bool m_Initialized = false;
	};
}