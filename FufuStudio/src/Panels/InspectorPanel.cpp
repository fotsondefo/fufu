#include "Panels/InspectorPanel.h"
#include <Project/Components.h>
#include "Application/Application.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include "Helpers/FontIcons.h"
#include "Helpers/AssetDrop.h"
#include "Commands/CommandHistory.h"
#include "Commands/ComponentCommands.h"
#include <nfd.hpp>

namespace FufuStudio 
{

	// Helper � dessine un label align� � gauche + widget � droite
	static void drawVec3Control(const char* label, glm::vec3& values, float resetValue = 0.f, float speed = 0.01f)
	{
		ImGui::PushID(label);

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, 100.f);
		ImGui::Text("%s", label);
		ImGui::NextColumn();

		ImGui::PushItemWidth(-1);
		ImGui::DragFloat3("##v", glm::value_ptr(values), speed);
		ImGui::PopItemWidth();

		// Double-clic pour reset
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			values = glm::vec3(resetValue);

		ImGui::Columns(1);
		ImGui::PopID();
	}

	// ----------------------------------------------------------------

	template<typename Component>
	void InspectorPanel::trackEdit(Fufu::Entity entity, std::optional<Component>& pending, EditorState& state)
	{
		if (ImGui::IsItemActivated() && !pending.has_value())
			pending = entity.getComponent<Component>();

		if (ImGui::IsItemDeactivatedAfterEdit() && pending.has_value())
		{
			state.commandHistory->executeCommand<ComponentEditCommand<Component>>(
				entity, *pending, entity.getComponent<Component>());
			pending.reset();
		}
	}

	template<typename T>
	void InspectorPanel::drawAddComponentButton(Fufu::Entity entity, const char* label, EditorState& state)
	{
		if (!entity.hasComponent<T>() && ImGui::MenuItem(label))
			state.commandHistory->executeCommand<ComponentAddCommand<T>>(entity);
	}

	// ----------------------------------------------------------------

	void InspectorPanel::onImGuiRender(EditorState& state)
	{
		ImGui::Begin(ICON_FA_SLIDERS " Inspector##inspector");

		Fufu::Entity entity = state.selection.primary();

		if (!entity || !entity.isValid())
		{
			ImGui::TextDisabled("No entity selected");
			ImGui::End();
			return;
		}

		if (state.selection.size() > 1)
			ImGui::TextDisabled("+%d more selected", static_cast<int>(state.selection.size()) - 1);

		drawTag(entity, state);
		ImGui::Separator();

		if (entity.hasComponent<Fufu::TransformComponent>())
			drawTransform(entity, state);

		if (entity.hasComponent<Fufu::MeshComponent>())
			drawMesh(entity, state);

		if (entity.hasComponent<Fufu::MaterialComponent>())
			drawMaterial(entity, state);

		if (entity.hasComponent<Fufu::CameraComponent>())
			drawCamera(entity, state);

		if (entity.hasComponent<Fufu::GroomComponent>())
			drawGroom(entity, state);

		if (entity.hasComponent<Fufu::LightComponent>())
			drawLight(entity, state);

		// Bouton "Add Component"
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Add Component", ImVec2(-1, 0)))
			ImGui::OpenPopup("AddComponentPopup");

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			drawAddComponentButton<Fufu::MeshComponent>(entity, "Mesh", state);
			drawAddComponentButton<Fufu::MaterialComponent>(entity, "Material", state);
			drawAddComponentButton<Fufu::CameraComponent>(entity, "Camera", state);

			// Groom a besoin d'un Mesh (surface porteuse) pour produire quoi que ce soit
			if (entity.hasComponent<Fufu::MeshComponent>())
				drawAddComponentButton<Fufu::GroomComponent>(entity, "Groom", state);

			drawAddComponentButton<Fufu::LightComponent>(entity, "Light", state);

			ImGui::EndPopup();
		}

		ImGui::End();
	}

	// ----------------------------------------------------------------

	void InspectorPanel::drawTag(Fufu::Entity entity, EditorState& state)
	{
		auto& tag = entity.getComponent<Fufu::TagComponent>().tag;

		char buf[256];
		strncpy_s(buf, tag.c_str(), sizeof(buf) - 1);

		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##tag", buf, sizeof(buf)))
			tag = std::string(buf);
		trackEdit(entity, m_PendingTag, state);
		ImGui::PopItemWidth();
	}

	void InspectorPanel::drawTransform(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& t = entity.getComponent<Fufu::TransformComponent>();

		glm::vec3 posBefore = t.position;
		glm::vec3 rotBefore = t.rotation;
		glm::vec3 scaBefore = t.scale;

		drawVec3Control("Position", t.position, 0.f, 0.05f);
		trackEdit(entity, m_PendingTransform, state);
		drawVec3Control("Rotation", t.rotation, 0.f, 0.01f);
		trackEdit(entity, m_PendingTransform, state);
		drawVec3Control("Scale", t.scale, 1.f, 0.01f);
		trackEdit(entity, m_PendingTransform, state);

		// Reset accumulation si la cam�ra principale a boug�
		bool changed = t.position != posBefore || t.rotation != rotBefore || t.scale != scaBefore;

		if (changed && entity.hasComponent<Fufu::CameraComponent>() && entity.getComponent<Fufu::CameraComponent>().primary)
			Fufu::Application::get().getRenderer().resetAccumulation();
		else if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();
	}

	void InspectorPanel::drawMesh(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& mesh = entity.getComponent<Fufu::MeshComponent>();

		char buf[512];
		strncpy_s(buf, mesh.meshPath.c_str(), sizeof(buf) - 1);

		ImGui::Text("Path");
		ImGui::SameLine();
		ImGui::PushItemWidth(-60.f);
		if (ImGui::InputText("##meshpath", buf, sizeof(buf),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			mesh.meshPath = std::string(buf);
			mesh.meshID = 0; // invalider le cache UUID
			Fufu::Application::get().getRenderer().resetAccumulation();
		}
		trackEdit(entity, m_PendingMesh, state);

		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Mesh)
				state.commandHistory->executeCommand<SetMeshCommand>(entity, meta->sourcePath.string(), meta->uuid.value());
			ImGui::EndDragDropTarget();
		}
		ImGui::PopItemWidth();

		ImGui::SameLine();
		if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN "##browsemesh"))
		{
			NFD::Guard nfdGuard;
			NFD::UniquePath path;
			nfdfilteritem_t filter = { "Mesh", "obj,fbx,gltf,glb,dae" };

			auto& pm = Fufu::Application::get().getProjectManager();
			std::filesystem::path defaultDir = pm.hasProject()
				? pm.getCurrentProject().getInfo().assetsDir()
				: std::filesystem::current_path();

			if (NFD::OpenDialog(path, &filter, 1, defaultDir.string().c_str()) == NFD_OKAY)
				state.commandHistory->executeCommand<SetMeshCommand>(entity, std::string(path.get()), uint64_t(0));
		}

		ImGui::TextDisabled("UUID: %llu", mesh.meshID);

		// Bouton Remove
		if (ImGui::SmallButton("Remove##mesh"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::MeshComponent>>(entity);
	}

	void InspectorPanel::drawMaterial(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& mat = entity.getComponent<Fufu::MaterialComponent>();
		bool  changed = false;

		// Albedo avec color picker
		ImGui::Text("Albedo");
		ImGui::SameLine();
		float col[4] = { mat.albedo.r, mat.albedo.g, mat.albedo.b, mat.albedo.a };
		if (ImGui::ColorEdit4("##albedo", col))
		{
			mat.albedo = glm::vec4(col[0], col[1], col[2], col[3]);
			changed = true;
		}
		trackEdit(entity, m_PendingMaterial, state);

		if (ImGui::SliderFloat("Metallic##mat", &mat.metallic, 0.f, 1.f)) changed = true;
		trackEdit(entity, m_PendingMaterial, state);
		if (ImGui::SliderFloat("Roughness##mat", &mat.roughness, 0.f, 1.f)) changed = true;
		trackEdit(entity, m_PendingMaterial, state);
		if (ImGui::SliderFloat("Emissive##mat", &mat.emissive, 0.f, 20.f)) changed = true;
		trackEdit(entity, m_PendingMaterial, state);

		if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();

		if (ImGui::SmallButton("Remove##mat"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::MaterialComponent>>(entity);
	}

	void InspectorPanel::drawCamera(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& cam = entity.getComponent<Fufu::CameraComponent>();
		bool changed = false;

		// Primary toggle
		if (ImGui::Checkbox("Primary", &cam.primary)) changed = true;
		trackEdit(entity, m_PendingCamera, state);

		// Projection
		int proj = static_cast<int>(cam.projection);
		if (ImGui::RadioButton("Perspective", &proj, 0))
		{
			cam.projection = Fufu::CameraProjection::Perspective;  changed = true;
		}
		trackEdit(entity, m_PendingCamera, state);
		ImGui::SameLine();
		if (ImGui::RadioButton("Orthographic", &proj, 1))
		{
			cam.projection = Fufu::CameraProjection::Orthographic; changed = true;
		}
		trackEdit(entity, m_PendingCamera, state);

		if (cam.projection == Fufu::CameraProjection::Perspective)
		{
			float fovDeg = glm::degrees(cam.fov);
			if (ImGui::SliderFloat("FOV", &fovDeg, 10.f, 120.f))
			{
				cam.fov = glm::radians(fovDeg); changed = true;
			}
			trackEdit(entity, m_PendingCamera, state);
		}
		else
		{
			if (ImGui::SliderFloat("Ortho Size", &cam.orthoSize, 0.1f, 100.f))
				changed = true;
			trackEdit(entity, m_PendingCamera, state);
		}

		if (ImGui::DragFloat("Near", &cam.nearPlane, 0.01f, 0.001f, 10.f))  changed = true;
		trackEdit(entity, m_PendingCamera, state);
		if (ImGui::DragFloat("Far", &cam.farPlane, 1.f, 1.f, 10000.f)) changed = true;
		trackEdit(entity, m_PendingCamera, state);

		if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();

		if (ImGui::SmallButton("Remove##cam"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::CameraComponent>>(entity);
	}

	void InspectorPanel::drawGroom(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Groom", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		if (!entity.hasComponent<Fufu::MeshComponent>())
			ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.f), "Needs a Mesh component to grow hair from.");

		auto& groom = entity.getComponent<Fufu::GroomComponent>();
		bool changed = false;

		ImGui::Text("Color");
		ImGui::SameLine();
		float col[4] = { groom.color.r, groom.color.g, groom.color.b, groom.color.a };
		if (ImGui::ColorEdit4("##groomcolor", col))
		{
			groom.color = glm::vec4(col[0], col[1], col[2], col[3]);
			changed = true;
		}
		trackEdit(entity, m_PendingGroom, state);

		if (ImGui::DragInt("Strands", &groom.strandCount, 4.f, 0, 20000)) changed = true;
		trackEdit(entity, m_PendingGroom, state);
		if (ImGui::DragInt("Segments", &groom.segments, 0.1f, 1, 12)) changed = true;
		trackEdit(entity, m_PendingGroom, state);
		if (ImGui::DragFloat("Length", &groom.length, 0.005f, 0.01f, 5.f)) changed = true;
		trackEdit(entity, m_PendingGroom, state);
		if (ImGui::DragFloat("Thickness", &groom.thickness, 0.001f, 0.001f, 0.5f)) changed = true;
		trackEdit(entity, m_PendingGroom, state);
		if (ImGui::DragFloat("Gravity", &groom.gravity, 0.005f, -2.f, 2.f)) changed = true;
		trackEdit(entity, m_PendingGroom, state);
		if (ImGui::DragFloat("Randomness", &groom.randomness, 0.005f, 0.f, 1.f)) changed = true;
		trackEdit(entity, m_PendingGroom, state);

		int seed = static_cast<int>(groom.seed);
		if (ImGui::DragInt("Seed", &seed, 1.f, 0, 1000000))
		{
			groom.seed = static_cast<uint32_t>(seed < 0 ? 0 : seed);
			changed = true;
		}
		trackEdit(entity, m_PendingGroom, state);

		if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();

		if (ImGui::SmallButton("Remove##groom"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::GroomComponent>>(entity);
	}

	void InspectorPanel::drawLight(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& light = entity.getComponent<Fufu::LightComponent>();
		bool changed = false;

		int typeIdx = static_cast<int>(light.type);
		if (ImGui::RadioButton("Sun (Directional)", &typeIdx, 0)) changed = true;
		ImGui::SameLine();
		if (ImGui::RadioButton("Point", &typeIdx, 1)) changed = true;
		if (changed) light.type = static_cast<Fufu::LightType>(typeIdx);
		trackEdit(entity, m_PendingLight, state);

		if (light.type == Fufu::LightType::Directional)
			ImGui::TextDisabled("Rotate this entity to change the light direction.");
		else
			ImGui::TextDisabled("Move this entity to change the light position.");

		ImGui::Text("Color");
		ImGui::SameLine();
		float col[3] = { light.color.r, light.color.g, light.color.b };
		if (ImGui::ColorEdit3("##lightcolor", col))
		{
			light.color = glm::vec3(col[0], col[1], col[2]);
			changed = true;
		}
		trackEdit(entity, m_PendingLight, state);

		if (light.type == Fufu::LightType::Directional)
			changed |= ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.f, 100.f);
		else
			changed |= ImGui::DragFloat("Intensity", &light.intensity, 1.f, 0.f, 10000.f);
		trackEdit(entity, m_PendingLight, state);

		if (light.type == Fufu::LightType::Directional)
		{
			float angularDeg = glm::degrees(light.radius);
			if (ImGui::DragFloat("Softness (°)", &angularDeg, 0.01f, 0.f, 20.f, "%.3f"))
			{
				light.radius = glm::radians(angularDeg);
				changed = true;
			}
		}
		else
		{
			if (ImGui::DragFloat("Radius", &light.radius, 0.005f, 0.f, 20.f)) changed = true;
		}
		trackEdit(entity, m_PendingLight, state);

		if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();

		if (ImGui::SmallButton("Remove##light"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::LightComponent>>(entity);
	}

}