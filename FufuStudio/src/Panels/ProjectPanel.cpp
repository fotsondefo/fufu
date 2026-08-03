#include "Panels/ProjectPanel.h"
#include "Application/Application.h"
#include "Project/Assets/Asset.h"
#include "Project/WorldSettings.h"
#include "Helpers/FontIcons.h"
#include "Commands/CommandHistory.h"
#include <imgui.h>
#include <algorithm>
#include <cstdlib>


#pragma region Icons

#define ICON_MESH     ICON_FA_CUBE
#define ICON_TEXTURE  ICON_FA_FILE_IMAGE_O
#define ICON_SHADER   ICON_FA_CODE
#define ICON_SCENE    ICON_FA_FILM
#define ICON_FOLDER   ICON_FA_FOLDER

#pragma endregion

namespace FufuStudio 
{

	static const char* assetTypeIcon(Fufu::AssetType type)
	{
		switch (type)
		{
		case Fufu::AssetType::Mesh:    return ICON_MESH;
		case Fufu::AssetType::Texture: return ICON_TEXTURE;
		case Fufu::AssetType::Shader:  return ICON_SHADER;
		default:                       return ICON_FA_FILE;
		}
	}

	static const char* assetTypeName(Fufu::AssetType type)
	{
		switch (type)
		{
		case Fufu::AssetType::Mesh:    return "Mesh";
		case Fufu::AssetType::Texture: return "Texture";
		case Fufu::AssetType::Shader:  return "Shader";
		default:                       return "Unknown";
		}
	}

	void ProjectPanel::onImGuiRender(EditorState& state)
	{
		ImGui::Begin(ICON_FA_FOLDER_OPEN " Project##project");

		auto& pm = Fufu::Application::get().getProjectManager();

		if (!pm.hasProject())
		{
			ImGui::TextDisabled("No project open");
			ImGui::End();
			return;
		}

		drawProjectHeader();
		ImGui::Separator();

		if (ImGui::BeginTabBar("ProjectTabs"))
		{
			if (ImGui::BeginTabItem(ICON_FA_FILM " Scenes"))
			{
				drawSceneList(state);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(ICON_FA_CUBE " Assets"))
			{
				drawAssetBrowser(state);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	void ProjectPanel::drawProjectHeader()
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		auto& proj = pm.getCurrentProject();

		/*ImGui::PushFont(
			Fufu::Application::get().getProjectManager().hasProject() ? nullptr : nullptr
		);*/

		ImGui::Text("%s  %s", ICON_CI_PROJECT, proj.getName().c_str());

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.f);

		if (ImGui::SmallButton(ICON_FA_FLOPPY_O " Save"))
			pm.saveCurrentProject();

		ImGui::TextDisabled("%s", proj.getRootDir().string().c_str());

		// Unit system picker
		auto& ws = proj.getWorldSettings();
		static const char* unitNames[] = { "Meters (m)", "Centimeters (cm)", "Millimeters (mm)" };
		int unitIdx = static_cast<int>(ws.lengthUnit);
		ImGui::SetNextItemWidth(160.f);
		if (ImGui::Combo("Units##projunit", &unitIdx, unitNames, 3))
		{
			ws.lengthUnit = static_cast<Fufu::LengthUnit>(unitIdx);
			pm.saveCurrentProject();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Display unit for positions in the Inspector.\n"
				"Internal representation is always in meters.");
	}

	void ProjectPanel::drawSceneList(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		auto& sm = pm.getCurrentProject().getSceneManager();

		// New Scene Button
		if (ImGui::Button(ICON_FA_PLUS " New Scene"))
		{
			// Unique name: "New Scene" always collided on the same map key,
			// silently overwriting the previous one instead of
			// creating a new one.
			std::string baseName = "New Scene";
			std::string name = baseName;
			int suffix = 1;
			while (sm.getLoadedScenes().count(name))
				name = baseName + " " + std::to_string(suffix++);

			auto newScene = sm.newScene(name);
			sm.setActiveScene(name);
			state.selection.clear();
			if (state.commandHistory) state.commandHistory->clear();
			state.syncToActiveScene();
			Fufu::Application::get().getRenderer().resetAccumulation();

			// Immediate save: without this the scene would not survive an
			// app close until an explicit Save is done
			// (see also Project::saveAllLoadedScenes, the safety net at
			// close — this additionally saves it right at creation).
			auto& proj = pm.getCurrentProject();
			std::filesystem::path path = proj.getInfo().scenesDir() / (name + ".fufuscene");
			if (sm.saveScene(newScene, path))
			{
				std::filesystem::path rel = std::filesystem::relative(path, proj.getRootDir());
				proj.registerScene(rel.generic_string());
			}
		}

		ImGui::Separator();

		// Deferred actions: renaming/unloading a scene while iterating
		// the map would corrupt it (iterator invalidation).
		bool doRename = false;
		std::string renameFrom, renameTo;
		bool doUnload = false;
		std::string unloadName;

		// Loaded Scenes
		for (auto&[name, scene] : sm.getLoadedScenes())
		{
			bool isActive = (sm.getActiveScene() == scene);
			bool isRenaming = m_RenamingScene && m_RenamingSceneName == name;

			if (isRenaming)
			{
				ImGui::PushID(name.c_str());
				ImGui::SetNextItemWidth(-1);
				ImGui::SetKeyboardFocusHere();
				bool submitted = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

				if (submitted)
				{
					renameFrom = name;
					renameTo = m_RenameBuffer;
					doRename = true;
					m_RenamingScene = false;
				}
				else if (ImGui::IsItemDeactivated())
				{
					m_RenamingScene = false; // click elsewhere / Escape: cancel
				}
				ImGui::PopID();
				continue;
			}

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth;

			if (isActive)
				flags |= ImGuiTreeNodeFlags_Selected;

			ImGui::TreeNodeEx((ICON_SCENE " " + name).c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				sm.setActiveScene(name);
				state.selection.clear();
				if (state.commandHistory) state.commandHistory->clear();
				state.syncToActiveScene();

				Fufu::Application::get().getRenderer().resetAccumulation();
			}

			// Contextual menu
			if (ImGui::BeginPopupContextItem(("scene_ctx_" + name).c_str()))
			{
				if (ImGui::MenuItem(ICON_FA_FLOPPY_O " Save"))
				{
					auto& proj = pm.getCurrentProject();
					auto path = proj.getInfo().scenesDir() / (name + ".fufuscene");
					if (sm.saveScene(scene, path))
					{
						std::filesystem::path rel = std::filesystem::relative(path, proj.getRootDir());
						proj.registerScene(rel.generic_string());
					}
				}
				if (ImGui::MenuItem(ICON_FA_PENCIL " Rename"))
				{
					m_RenamingScene = true;
					m_RenamingSceneName = name;
					strncpy_s(m_RenameBuffer, name.c_str(), sizeof(m_RenameBuffer) - 1);
				}
				if (ImGui::MenuItem(ICON_FA_TRASH " Unload"))
				{
					unloadName = name;
					doUnload = true;
				}

				ImGui::EndPopup();
			}

			// "active" Badge
			if (isActive)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(active)");
			}
		}

		if (doRename && !renameTo.empty() && renameFrom != renameTo)
			sm.renameScene(renameFrom, renameTo);

		if (doUnload)
		{
			// The scene (and its entities) may be destroyed here: clearing the
			// selection and undo/redo history avoids keeping a
			// Fufu::Entity whose Scene* is dangling (this was causing
			// the app to crash on the next selection access).
			state.selection.clear();
			if (state.commandHistory) state.commandHistory->clear();
			sm.unloadScene(unloadName);
			Fufu::Application::get().getRenderer().resetAccumulation();
		}
	}

	// ── Asset browser helpers ─────────────────────────────────────────────────

	static const char* iconForExtension(const std::filesystem::path& ext)
	{
		std::string e = ext.string();
		std::transform(e.begin(), e.end(), e.begin(), ::tolower);
		if (e == ".obj" || e == ".fbx" || e == ".gltf" || e == ".glb" ||
		    e == ".dae" || e == ".fmesh")
			return ICON_MESH;
		if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" ||
		    e == ".bmp" || e == ".hdr" || e == ".exr")
			return ICON_TEXTURE;
		if (e == ".vert" || e == ".frag" || e == ".comp" || e == ".glsl")
			return ICON_SHADER;
		if (e == ".fufuscene")
			return ICON_SCENE;
		return ICON_FA_FILE;
	}

	static Fufu::AssetType typeForExtension(const std::filesystem::path& ext)
	{
		std::string e = ext.string();
		std::transform(e.begin(), e.end(), e.begin(), ::tolower);
		if (e == ".obj" || e == ".fbx" || e == ".gltf" || e == ".glb" ||
		    e == ".dae" || e == ".fmesh")
			return Fufu::AssetType::Mesh;
		if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" ||
		    e == ".bmp" || e == ".hdr" || e == ".exr")
			return Fufu::AssetType::Texture;
		if (e == ".vert" || e == ".frag" || e == ".comp" || e == ".glsl")
			return Fufu::AssetType::Shader;
		return Fufu::AssetType::None;
	}

	void ProjectPanel::drawDirTree(const std::filesystem::path& dir)
	{
		namespace fs = std::filesystem;
		if (!fs::exists(dir) || !fs::is_directory(dir)) return;

		for (auto& entry : fs::directory_iterator(dir))
		{
			if (!entry.is_directory()) continue;
			const auto& p = entry.path();
			std::string name = p.filename().string();

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
			                          ImGuiTreeNodeFlags_SpanAvailWidth;
			if (m_BrowserDir == p)
				flags |= ImGuiTreeNodeFlags_Selected;

			bool hasSubDirs = false;
			for (auto& sub : fs::directory_iterator(p))
				if (sub.is_directory()) { hasSubDirs = true; break; }
			if (!hasSubDirs)
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			bool open = ImGui::TreeNodeEx((ICON_FOLDER " " + name).c_str(), flags);
			if (ImGui::IsItemClicked())
				m_BrowserDir = p;

			if (open && hasSubDirs)
			{
				drawDirTree(p);
				ImGui::TreePop();
			}
		}
	}

	void ProjectPanel::drawAssetBrowser(EditorState& /*state*/)
	{
		namespace fs = std::filesystem;

		auto& pm  = Fufu::Application::get().getProjectManager();
		auto& am  = pm.getCurrentProject().getAssetManager();
		auto  root = pm.getCurrentProject().getInfo().assetsDir();

		// Initialise the browser to the assets root on first open
		if (m_BrowserDir.empty() || !fs::exists(m_BrowserDir))
			m_BrowserDir = root;

		// ── Toolbar ───────────────────────────────────────────────────────────
		if (ImGui::Button(ICON_CI_REFRESH))
			am.scanDirectory();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Rescan assets directory");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.f);
		ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " search", m_SearchBuf, sizeof(m_SearchBuf));
		ImGui::SameLine();
		ImGui::TextDisabled("%zu registered", am.assetCount());
		ImGui::Separator();

		// ── Two-pane layout ───────────────────────────────────────────────────
		float leftW = 140.f;
		float avail = ImGui::GetContentRegionAvail().x;

		// Left: directory tree
		ImGui::BeginChild("##dirTree", { leftW, 0 }, true);
		{
			ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow |
			                              ImGuiTreeNodeFlags_DefaultOpen  |
			                              ImGuiTreeNodeFlags_SpanAvailWidth;
			if (m_BrowserDir == root)
				rootFlags |= ImGuiTreeNodeFlags_Selected;

			bool rootOpen = ImGui::TreeNodeEx(ICON_FOLDER " assets", rootFlags);
			if (ImGui::IsItemClicked())
				m_BrowserDir = root;
			if (rootOpen)
			{
				drawDirTree(root);
				ImGui::TreePop();
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// Right: file grid for current directory
		ImGui::BeginChild("##fileGrid", { avail - leftW - 8.f, 0 }, false);
		{
			std::string filter(m_SearchBuf);
			std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

			if (!fs::exists(m_BrowserDir))
			{
				ImGui::TextDisabled("(directory not found)");
			}
			else
			{
				// Breadcrumb
				auto rel = fs::relative(m_BrowserDir, root.parent_path());
				ImGui::TextDisabled("%s", rel.string().c_str());
				ImGui::Separator();

				for (auto& entry : fs::directory_iterator(m_BrowserDir))
				{
					if (entry.is_directory()) continue;

					const auto& p   = entry.path();
					std::string name = p.filename().string();
					std::string nameLo = name;
					std::transform(nameLo.begin(), nameLo.end(), nameLo.begin(), ::tolower);

					if (!filter.empty() && nameLo.find(filter) == std::string::npos)
						continue;

					// Skip .fmeta sidecar files
					if (p.extension() == ".fmeta") continue;

					const char* icon = iconForExtension(p.extension());
					bool selected    = (m_SelectedAsset == p);

					ImGuiTreeNodeFlags flags =
						ImGuiTreeNodeFlags_Leaf |
						ImGuiTreeNodeFlags_NoTreePushOnOpen |
						ImGuiTreeNodeFlags_SpanAvailWidth;
					if (selected) flags |= ImGuiTreeNodeFlags_Selected;

					ImGui::TreeNodeEx((std::string(icon) + "  " + name).c_str(), flags);

					if (ImGui::IsItemClicked())
						m_SelectedAsset = p;

					// Tooltip
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("%s", p.string().c_str());
						ImGui::Text("Size: %llu KB",
							static_cast<unsigned long long>(fs::file_size(p) / 1024));
						ImGui::EndTooltip();
					}

					// Right-click context menu
					if (ImGui::BeginPopupContextItem(("fctx_" + name).c_str()))
					{
						auto assetType = typeForExtension(p.extension());
						if (assetType != Fufu::AssetType::None)
						{
							if (ImGui::MenuItem(ICON_FA_PLUS_CIRCLE " Import as Asset"))
							{
								am.registerAsset(p, assetType);
								am.scanDirectory();
							}
						}
						if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Show in Explorer"))
						{
							// Open the containing folder in Windows Explorer
							std::string cmd = "explorer /select,\"" + p.string() + "\"";
							std::system(cmd.c_str());
						}
						ImGui::EndPopup();
					}

					// Drag source — drop onto entity (texture/mesh slots in Inspector)
					if (ImGui::BeginDragDropSource())
					{
						// Emit the file path string; Inspector accepts DND_FILEPATH
						std::string pathStr = p.string();
						ImGui::SetDragDropPayload("DND_FILEPATH",
							pathStr.c_str(), pathStr.size() + 1);
						ImGui::Text("%s  %s", icon, name.c_str());
						ImGui::EndDragDropSource();
					}
				}
			}
		}
		ImGui::EndChild();
	}

}