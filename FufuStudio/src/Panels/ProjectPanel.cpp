#include "Panels/ProjectPanel.h"
#include "Application/Application.h"
#include "Project/Assets/Asset.h"
#include "Helpers/FontIcons.h"
#include "Commands/CommandHistory.h"
#include <imgui.h>


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
				drawAssetTree(state);
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
	}

	void ProjectPanel::drawSceneList(EditorState& state)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		auto& sm = pm.getCurrentProject().getSceneManager();

		// New Scene Button
		if (ImGui::Button(ICON_FA_PLUS " New Scene"))
		{
			// Nom unique : "New Scene" collisionnait toujours sur la même clé
			// de map, écrasant silencieusement la précédente au lieu d'en
			// créer une nouvelle.
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

			// Sauvegarde immédiate : sans ça la scène ne survivrait pas à une
			// fermeture de l'appli tant qu'on n'a pas fait Save explicitement
			// (voir aussi Project::saveAllLoadedScenes, le filet de sécurité à
			// la fermeture — ceci l'enregistre en plus dès la création).
			auto& proj = pm.getCurrentProject();
			std::filesystem::path path = proj.getInfo().scenesDir() / (name + ".fufuscene");
			if (sm.saveScene(newScene, path))
			{
				std::filesystem::path rel = std::filesystem::relative(path, proj.getRootDir());
				proj.registerScene(rel.generic_string());
			}
		}

		ImGui::Separator();

		// Actions différées : renommer/décharger une scène pendant qu'on
		// itère la map la corromprait (invalidation d'itérateur).
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
					m_RenamingScene = false; // clic ailleurs / Échap : annule
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
			// La scène (et ses entités) peut être détruite ici : vider la
			// sélection et l'historique undo/redo évite de garder un
			// Fufu::Entity dont le Scene* est pendant (c'est ce qui faisait
			// planter l'appli au prochain accès à la sélection).
			state.selection.clear();
			if (state.commandHistory) state.commandHistory->clear();
			sm.unloadScene(unloadName);
			Fufu::Application::get().getRenderer().resetAccumulation();
		}
	}

	void ProjectPanel::drawAssetTree(EditorState& /*state*/)
	{
		auto& pm = Fufu::Application::get().getProjectManager();
		auto& am = pm.getCurrentProject().getAssetManager();

		// Refresh button
		if (ImGui::Button(ICON_CI_REFRESH " Rescan"))
			am.scanDirectory();

		ImGui::SameLine();
		ImGui::TextDisabled("%zu assets", am.assetCount());
		ImGui::Separator();

		// Group by type
		struct AssetGroup
		{
			const char* icon;
			const char* label;
			Fufu::AssetType type;
			std::vector<const Fufu::AssetMeta*> metas;
		};

		std::array<AssetGroup, 3> groups = { {
			{ ICON_MESH,    "Meshes",   Fufu::AssetType::Mesh,    {} },
			{ ICON_TEXTURE, "Textures", Fufu::AssetType::Texture, {} },
			{ ICON_SHADER,  "Shaders",  Fufu::AssetType::Shader,  {} },
		} };

		// Fill the groups
		for (auto&[uuid, asset] : am.getPool())
		{
			const auto& meta = asset->getMeta();
			for (auto& group : groups)
				if (meta.type == group.type)
					group.metas.push_back(&meta);
		}

		// Display
		for (auto& group : groups)
		{
			std::string header = std::string(group.icon) + "  " + group.label + " (" + std::to_string(group.metas.size()) + ")";

			if (ImGui::CollapsingHeader(header.c_str(),
				ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const auto* meta : group.metas)
				{
					std::string filename =
						meta->sourcePath.filename().string();

					bool selected = (m_SelectedAsset == meta->sourcePath);

					ImGuiTreeNodeFlags flags =
						ImGuiTreeNodeFlags_Leaf |
						ImGuiTreeNodeFlags_NoTreePushOnOpen |
						ImGuiTreeNodeFlags_SpanAvailWidth;

					if (selected) flags |= ImGuiTreeNodeFlags_Selected;

					ImGui::TreeNodeEx(
						(std::string(assetTypeIcon(meta->type)) + "  " + filename).c_str(), flags
					);

					if (ImGui::IsItemClicked())
						m_SelectedAsset = meta->sourcePath;

					// Tooltip
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("Path: %s", meta->sourcePath.string().c_str());
						ImGui::Text("Type: %s", assetTypeName(meta->type));
						ImGui::Text("UUID: %llu", meta->uuid.value());
						ImGui::Text("State: %s", meta->state == Fufu::AssetState::Loaded ? "Loaded" : "Unloaded");
						ImGui::EndTooltip();
					}

					// Drag source - to drop an asset onto an entity
					if (ImGui::BeginDragDropSource())
					{
						uint64_t uuidVal = meta->uuid.value();
						ImGui::SetDragDropPayload(
							"ASSET_UUID", &uuidVal, sizeof(uint64_t));
						ImGui::Text("%s  %s",
							assetTypeIcon(meta->type), filename.c_str());
						ImGui::EndDragDropSource();
					}
				}
			}
		}
	}

}