#include "Panels/ViewportPanel.h"
#include "Helpers/FontIcons.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <Application/Application.h>
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
		if (state.viewportFocused)
			handleCameraInput(state, deltaTime);

		syncCameraToScene(state);
	}

	void ViewportPanel::handleCameraInput(EditorState& state, float deltaTime)
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Fufu::Application::get().getWindow().getNativeWindow());

		// --- Rotation souris (clic droit maintenu) ---
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
		{
			double mx, my;
			glfwGetCursorPos(window, &mx, &my);
			glm::vec2 mousePos = { static_cast<float>(mx), static_cast<float>(my) };

			if (m_FirstMouse)
			{
				m_LastMousePos = mousePos;
				m_FirstMouse = false;
				m_RightClickDragDistance = 0.f;
			}

			glm::vec2 delta = mousePos - m_LastMousePos;
			m_LastMousePos = mousePos;
			m_RightClickDragDistance += glm::length(delta);

			// Signe cohérent avec le forward dérivé du quaternion (voir plus bas) :
			// souris à droite/haut doit tourner la vue à droite/vers le haut.
			state.cameraRotation.y -= delta.x * state.cameraLookSpeed * deltaTime; // yaw
			state.cameraRotation.x -= delta.y * state.cameraLookSpeed * deltaTime; // pitch

			// Clamp pitch pour éviter le gimbal lock
			state.cameraRotation.x = std::clamp(
				state.cameraRotation.x,
				glm::radians(-89.f),
				glm::radians(89.f)
			);

			m_Renderer.resetAccumulation();
			m_RightMouseWasDown = true;
		}
		else
		{
			m_FirstMouse = true;

			// Clic droit relâché sans drag significatif (rotation caméra) :
			// menu contextuel plutôt qu'une simple rotation qui n'a pas eu lieu.
			if (m_RightMouseWasDown && m_RightClickDragDistance < 4.f && state.viewportHovered)
				m_OpenContextMenu = true;

			m_RightMouseWasDown = false;
		}

		// --- Déplacement WASD ---
		// Dérivé du même quaternion que TransformComponent::getTransform()
		// (utilisé par le rendu et le gizmo), pour que le déplacement corresponde
		// toujours à ce qui est effectivement affiché à l'écran.
		float pitch = state.cameraRotation.x;
		float yaw = state.cameraRotation.y;

		glm::quat camRotation = glm::quat(glm::vec3(pitch, yaw, 0.f));
		glm::vec3 forward = glm::normalize(camRotation * glm::vec3(0.f, 0.f, -1.f));
		glm::vec3 right = glm::normalize(camRotation * glm::vec3(1.f, 0.f, 0.f));
		glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);

		float speed = state.cameraMoveSpeed * deltaTime;

		// Shift pour accélérer
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 3.f;

		bool moved = false;
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { state.cameraPosition += forward * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { state.cameraPosition -= forward * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { state.cameraPosition -= right * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { state.cameraPosition += right * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { state.cameraPosition += up * speed; moved = true; }
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { state.cameraPosition -= up * speed; moved = true; }

		if (moved) m_Renderer.resetAccumulation();
	}

	void ViewportPanel::syncCameraToScene(EditorState& state)
	{
		if (!state.getActiveScene()) return;

		Fufu::Entity cam = state.getActiveScene()->getPrimaryCamera();
		if (!cam) return;

		auto& t = cam.getComponent<Fufu::TransformComponent>();
		t.position = state.cameraPosition;
		t.rotation = state.cameraRotation;
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

		m_OrientationGizmo.render(state);
		m_OrientationGizmo.handleShortcuts(state);

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
