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

		// Resync caméra si la scène a changé depuis la dernière frame
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

		// Toujours pousser vers la scène pour que les outils (gizmo, pick...)
		// voient la position à jour même sans input ce frame.
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

		// Détecter un redimensionnement du viewport
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
			ImVec2(0, 1),   // UV flip vertical – OpenGL origin bas-gauche
			ImVec2(1, 0)
		);

		// Barres d'outils flottantes, dessinées PAR-DESSUS l'image (après elle
		// dans l'ordre d'appel = au-dessus dans l'ordre de rendu ImGui) via des
		// positions écran absolues : elles ne réservent aucun espace de layout,
		// l'image garde toute la taille du panneau.
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

		// Déposer un asset Mesh depuis le ProjectPanel crée une nouvelle entité
		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Mesh)
			{
				auto scene = state.getActiveScene();
				if (scene)
				{
					std::string path = meta->sourcePath.string();
					uint64_t id = meta->uuid.value();

					auto* cmd = state.commandHistory->executeCommand<EntityCreateCommand>(
						scene, meta->sourcePath.stem().string(), Fufu::Entity{},
						[path, id](Fufu::Entity e)
						{
							e.addComponent<Fufu::MeshComponent>(path, id);
							e.addComponent<Fufu::MaterialComponent>();
						});

					state.selection.select(cmd->getEntity());
					m_Renderer.resetAccumulation();
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (activeTool)
		{
			activeTool->onViewportOverlay(state);
			activeTool->onShortcuts(state);
		}

		// Overlay : infos accumulation, sous les barres d'outils
		if (m_Renderer.getSettings().mode == Fufu::RenderMode::Accumulation)
		{
			ImVec2 overlayPos = ImVec2(imagePos.x + 8.f, imagePos.y + 76.f);
			ImGui::GetWindowDrawList()->AddText(
				overlayPos, IM_COL32(255, 255, 255, 200),
				("Samples: " + std::to_string(m_Renderer.getAccumulatedFrames())).c_str()
			);
		}

		// Indicateur de chargement en arrière-plan (import de mesh/texture,
		// construction de BVH — voir JobSystem/AssetManager) : sans ça, un
		// objet lourd glissé dans la scène met plusieurs secondes à apparaître
		// sans aucun retour visuel.
		int pendingJobs = Fufu::Application::get().getJobSystem().getPendingJobCount();
		if (pendingJobs > 0)
		{
			ImVec2 overlayPos = ImVec2(imagePos.x + 8.f, imagePos.y + 96.f);
			std::string text = "Loading " + std::to_string(pendingJobs) + " asset(s)...";
			ImGui::GetWindowDrawList()->AddText(overlayPos, IM_COL32(255, 210, 90, 220), text.c_str());
		}

		// FPS / temps de frame, en bas à gauche du viewport (voir Fufu::Profiler,
		// mesuré autour de toute la frame dans Application::run()).
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
							// Orientation par défaut façon "soleil" : incliné vers le bas
							// plutôt que (0,0,0) qui pointerait à l'horizontale.
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
							// L'intensité par défaut (pensée pour le soleil) serait quasi
							// invisible une fois atténuée en 1/distance² : valeurs adaptées
							// à une lampe de pièce à la place.
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
