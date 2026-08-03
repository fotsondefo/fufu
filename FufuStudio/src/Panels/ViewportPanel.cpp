#include "Panels/ViewportPanel.h"
#include "Helpers/FontIcons.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <Application/Application.h>
#include <Application/Profiler.h>
#include <Project/Components.h>
#include <Project/Assets/PrimitiveMeshes.h>
#include <algorithm>
#include "Tools/TransformGizmoTool.h"
#include "Tools/ModelingTool.h"
#include "Tools/SculptTool.h"
#include "Helpers/AssetDrop.h"
#include "Helpers/PrimitiveFactory.h"
#include "Commands/CommandHistory.h"
#include "Commands/EntityCommands.h"
#include <nfd.hpp>
#include <filesystem>
#include <algorithm>

namespace FufuStudio
{

	ViewportPanel::ViewportPanel(Fufu::Renderer& renderer)
		: m_Renderer(renderer)
		, m_OrientationGizmo(renderer)
	{
		m_ToolManager.registerTool(std::make_unique<TransformGizmoTool>(m_Renderer));
		m_ToolManager.registerTool(std::make_unique<ModelingTool>());
		m_ToolManager.registerTool(std::make_unique<SculptTool>(m_Renderer));
	}

	void ViewportPanel::onUpdate(EditorState& state, float deltaTime)
	{
		auto scene = state.getActiveScene();

		// Resync camera if the scene has changed since the last frame
		if (scene.get() != m_LastScene)
		{
			m_LastScene = scene.get();
			if (scene)
				m_CameraController.syncFromScene(*scene);
		}

		if (state.viewportFocused && scene)
		{
			bool moved = m_CameraController.onUpdate(deltaTime, true);
			if (moved)
			{
				m_CameraController.syncToScene(*scene);
				m_Renderer.resetAccumulation();
			}

			if (m_CameraController.consumeContextMenuRequest() && state.viewportHovered)
				m_OpenContextMenu = true;
		}
		else
		{
			m_CameraController.onUpdate(deltaTime, false);
		}

		// Always push to the scene so tools (gizmo, pick...)
		// see the up-to-date position even with no input this frame.
		if (scene)
			m_CameraController.syncToScene(*scene);
	}

	void ViewportPanel::syncCameraFromScene(EditorState& state)
	{
		auto scene = state.getActiveScene();
		if (!scene) return;
		m_CameraController.syncFromScene(*scene);
		m_LastScene = scene.get();
	}

	void ViewportPanel::onImGuiRender(EditorState& state)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::Begin(ICON_FA_EYE " Viewport##viewport");

		IEditorTool* activeTool = m_ToolManager.getActiveTool();

		state.viewportFocused = ImGui::IsWindowFocused();
		state.viewportHovered = ImGui::IsWindowHovered();

		// Detect a viewport resize
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		if (viewportSize.x > 0 && viewportSize.y > 0)
		{
			glm::vec2 newSize = { viewportSize.x, viewportSize.y };
			if (newSize != state.viewportSize)
			{
				state.viewportSize = newSize;
				m_Renderer.resize(
					static_cast<int>(newSize.x),
					static_cast<int>(newSize.y)
				);
			}
		}

		ImVec2 imagePos = ImGui::GetCursorScreenPos();
		state.viewportPos = { imagePos.x, imagePos.y };

		ImTextureID texID = reinterpret_cast<ImTextureID>(
			static_cast<uintptr_t>(m_Renderer.getOutputTextureID())
		);
		ImGui::Image(
			texID,
			viewportSize,
			ImVec2(0, 1),   // Vertical UV flip – OpenGL origin is bottom-left
			ImVec2(1, 0)
		);

		// Floating toolbars drawn ON TOP of the image (after it in the call
		// order = above it in ImGui render order) using absolute screen positions:
		// they reserve no layout space, the image keeps the full panel size.
		ImGui::SetCursorScreenPos(ImVec2(imagePos.x + 8.f, imagePos.y + 8.f));
		ImGui::BeginGroup();
		const auto& tools = m_ToolManager.getTools();
		for (std::size_t i = 0; i < tools.size(); ++i)
		{
			bool active = (activeTool == tools[i].get());
			if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f));
			if (ImGui::Button(tools[i]->getName()))
			{
				m_ToolManager.setActiveTool(i);
				activeTool = m_ToolManager.getActiveTool();
			}
			if (active) ImGui::PopStyleColor();
			ImGui::SameLine();
		}
		ImGui::EndGroup();

		ImGui::SetCursorScreenPos(ImVec2(imagePos.x + 8.f, imagePos.y + 42.f));
		ImGui::BeginGroup();
		if (activeTool)
			activeTool->onToolbar(state);
		ImGui::EndGroup();

		// Dropping a mesh or gaussian-splat .ply into the viewport.
		if (ImGui::BeginDragDropTarget())
		{
			// Path 1: already-registered asset (ASSET_UUID payload)
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Mesh)
			{
				beginMeshImportFromPath(meta->sourcePath.string());
				m_PendingImport.assetID = meta->uuid.value();
			}

			// Path 2: raw file path from the asset-browser grid (DND_FILEPATH payload)
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_FILEPATH"))
			{
				std::string filePath(static_cast<const char*>(payload->Data), payload->DataSize - 1);
				std::filesystem::path p(filePath);
				std::string ext = p.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				bool isMesh = (ext == ".obj" || ext == ".fbx" || ext == ".gltf" ||
				               ext == ".glb" || ext == ".dae" || ext == ".fmesh");
				if (isMesh && state.getActiveScene())
					beginMeshImportFromPath(filePath);

				// 3DGS — drop a .ply directly into the scene
				if (ext == ".ply" && state.getActiveScene())
				{
					auto scene = state.getActiveScene();
					std::string stem = std::filesystem::path(filePath).stem().string();
					state.commandHistory->executeCommand<EntityCreateCommand>(
						scene, stem, Fufu::Entity{},
						[filePath](Fufu::Entity e)
						{
							Fufu::GaussianSplatComponent gs;
							gs.path = filePath;
							e.addComponent<Fufu::GaussianSplatComponent>(gs);
						});
				}
			}

			ImGui::EndDragDropTarget();
		}

		drawImportModal(state);

		if (activeTool)
		{
			activeTool->onViewportOverlay(state);
			activeTool->onShortcuts(state);
		}

		// Overlay: accumulation info, below the toolbars
		if (m_Renderer.getSettings().mode == Fufu::RenderMode::Accumulation)
		{
			ImVec2 overlayPos = ImVec2(imagePos.x + 8.f, imagePos.y + 76.f);
			ImGui::GetWindowDrawList()->AddText(
				overlayPos, IM_COL32(255, 255, 255, 200),
				("Samples: " + std::to_string(m_Renderer.getAccumulatedFrames())).c_str()
			);
		}

		// Background loading indicator (mesh/texture import,
		// BVH construction — see JobSystem/AssetManager): without this, a
		// heavy object dragged into the scene takes several seconds to appear
		// with no visual feedback.
		int pendingJobs = Fufu::Application::get().getJobSystem().getPendingJobCount();
		if (pendingJobs > 0)
		{
			ImVec2 overlayPos = ImVec2(imagePos.x + 8.f, imagePos.y + 96.f);
			std::string text = "Loading " + std::to_string(pendingJobs) + " asset(s)...";
			ImGui::GetWindowDrawList()->AddText(overlayPos, IM_COL32(255, 210, 90, 220), text.c_str());
		}

		// FPS / frame time, at the bottom-left of the viewport (see Fufu::Profiler,
		// measured around the entire frame in Application::run()).
		{
			const auto& profilerFrame = Fufu::Profiler::get().getCurrentFrame();
			char fpsText[64];
			snprintf(fpsText, sizeof(fpsText), "%.0f FPS \xC2\xB7 %.2f ms",
				Fufu::Profiler::get().getFPS(), profilerFrame.cpuFrameTimeMs);

			ImVec2 overlayPos = ImVec2(imagePos.x + 8.f, imagePos.y + viewportSize.y - 24.f);
			ImGui::GetWindowDrawList()->AddText(overlayPos, IM_COL32(255, 255, 255, 220), fpsText);
		}

		m_OrientationGizmo.render(state, m_CameraController);
		m_OrientationGizmo.handleShortcuts(state, m_CameraController);

		drawContextMenu(state);

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewportPanel::drawContextMenu(EditorState& state)
	{
		if (m_OpenContextMenu)
		{
			ImGui::OpenPopup("ViewportContextMenu");
			m_OpenContextMenu = false;
		}

		if (!ImGui::BeginPopup("ViewportContextMenu"))
			return;

		auto scene = state.getActiveScene();

		if (ImGui::BeginMenu(ICON_FA_CUBE " Add Primitive"))
		{
			if (scene)
			{
				if (ImGui::MenuItem("Cube"))     createPrimitiveEntity(state, scene, "Cube", Fufu::PrimitiveMeshes::makeCube());
				if (ImGui::MenuItem("Plane"))    createPrimitiveEntity(state, scene, "Plane", Fufu::PrimitiveMeshes::makePlane());
				if (ImGui::MenuItem("Sphere"))   createPrimitiveEntity(state, scene, "Sphere", Fufu::PrimitiveMeshes::makeSphere());
				if (ImGui::MenuItem("Cylinder")) createPrimitiveEntity(state, scene, "Cylinder", Fufu::PrimitiveMeshes::makeCylinder());
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ICON_FA_LIGHTBULB_O " Add Light"))
		{
			if (scene)
			{
				if (ImGui::MenuItem("Sun (Directional)"))
				{
					state.commandHistory->executeCommand<EntityCreateCommand>(scene, "Sun", Fufu::Entity{},
						[](Fufu::Entity e)
						{
							// Default orientation like a "sun": tilted downward
							// rather than (0,0,0) which would point horizontally.
							auto& t = e.getComponent<Fufu::TransformComponent>();
							t.rotation = glm::vec3(glm::radians(-45.f), glm::radians(30.f), 0.f);
							e.addComponent<Fufu::LightComponent>();
						});
				}

				if (ImGui::MenuItem("Point"))
				{
					state.commandHistory->executeCommand<EntityCreateCommand>(scene, "Point Light", Fufu::Entity{},
						[](Fufu::Entity e)
						{
							Fufu::LightComponent light;
							light.type = Fufu::LightType::Point;
							// The default intensity (designed for a sun) would be nearly
							// invisible once attenuated by 1/distance²: adapted values
							// for a room lamp instead.
							light.intensity = 50.f;
							light.radius = 0.1f;
							e.addComponent<Fufu::LightComponent>(light);
						});
				}
			}
			ImGui::EndMenu();
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_CAMERA " Export Rendered Image..."))
			exportRenderedImage();

		ImGui::EndPopup();
	}

	void ViewportPanel::beginMeshImportFromPath(const std::string& path)
	{
		m_PendingImport.path    = path;
		m_PendingImport.assetID = 0;
		m_PendingImport.scale   = 1.f;
		m_PendingImport.open    = true;
	}

	void ViewportPanel::drawImportModal(EditorState& state)
	{
		// Trigger open on the first frame after a drop
		if (m_PendingImport.open)
		{
			ImGui::OpenPopup("Import Mesh##importmodal");
			m_PendingImport.open = false;
		}

		// Centre the modal in the viewport
		ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(340.f, 0.f), ImGuiCond_Appearing);

		if (!ImGui::BeginPopupModal("Import Mesh##importmodal", nullptr,
		                             ImGuiWindowFlags_AlwaysAutoResize))
			return;

		std::filesystem::path p(m_PendingImport.path);
		ImGui::TextUnformatted(p.filename().string().c_str());
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::DragFloat("Scale", &m_PendingImport.scale, 0.01f, 0.0001f, 1000.f, "%.4f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Uniform scale applied to the entity after import.\n"
				"FBX files from Blender are already scaled to metres\n"
				"by the importer — keep 1.0 unless you know otherwise.");

		// Quick-pick presets
		ImGui::Spacing();
		ImGui::TextDisabled("Presets:");
		ImGui::SameLine();
		if (ImGui::SmallButton("1.0"))  m_PendingImport.scale = 1.f;
		ImGui::SameLine();
		if (ImGui::SmallButton("0.01")) m_PendingImport.scale = 0.01f;
		ImGui::SameLine();
		if (ImGui::SmallButton("100"))  m_PendingImport.scale = 100.f;

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool doImport = false;
		if (ImGui::Button("Import", ImVec2(120.f, 0.f))) { doImport = true; }
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
		{
			m_PendingImport = {};
			ImGui::CloseCurrentPopup();
		}

		if (doImport)
		{
			auto scene = state.getActiveScene();
			if (scene)
			{
				auto& pm = Fufu::Application::get().getProjectManager();
				uint64_t assetID = m_PendingImport.assetID;

				// If the file wasn't registered yet, register it now
				if (assetID == 0 && pm.hasProject())
				{
					auto& am = pm.getCurrentProject().getAssetManager();
					Fufu::UUID uuid = am.registerAsset(
						std::filesystem::path(m_PendingImport.path),
						Fufu::AssetType::Mesh);
					assetID = uuid.value();
				}

				std::string path   = m_PendingImport.path;
				float       scale  = m_PendingImport.scale;
				std::string stem   = std::filesystem::path(path).stem().string();

				auto* cmd = state.commandHistory->executeCommand<EntityCreateCommand>(
					scene, stem, Fufu::Entity{},
					[path, assetID, scale](Fufu::Entity e)
					{
						e.addComponent<Fufu::MeshComponent>(path, assetID);
						e.addComponent<Fufu::MaterialComponent>();
						if (scale != 1.f)
						{
							auto& t = e.getComponent<Fufu::TransformComponent>();
							t.scale = glm::vec3(scale);
						}
					});

				state.selection.select(cmd->getEntity());
				m_Renderer.resetAccumulation();
			}

			m_PendingImport = {};
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	void ViewportPanel::exportRenderedImage()
	{
		NFD::Guard nfdGuard;
		NFD::UniquePath outPath;
		nfdfilteritem_t filter = { "PNG Image", "png" };

		nfdresult_t result = NFD::SaveDialog(outPath, &filter, 1, nullptr, "render.png");
		if (result != NFD_OKAY)
			return;

		std::filesystem::path path(outPath.get());
		if (path.extension().empty()) path += ".png";

		m_Renderer.exportImage(path);
	}

} // namespace FufuStudio
