#include "Panels/InspectorPanel.h"
#include <Project/Components.h>
#include "Application/Application.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace FufuStudio 
{

	// Helper — dessine un label aligné à gauche + widget à droite
	static void drawVec3Control(const char* label, glm::vec3& values,
		float resetValue = 0.f, float speed = 0.01f)
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

	void InspectorPanel::onImGuiRender(EditorState& state)
	{
		ImGui::Begin("Inspector");

		if (!state.selectedEntity || !state.selectedEntity.isValid())
		{
			ImGui::TextDisabled("No entity selected");
			ImGui::End();
			return;
		}

		Fufu::Entity entity = state.selectedEntity;

		drawTag(entity);
		ImGui::Separator();

		if (entity.hasComponent<Fufu::TransformComponent>())
			drawTransform(entity, state);

		if (entity.hasComponent<Fufu::MeshComponent>())
			drawMesh(entity);

		if (entity.hasComponent<Fufu::MaterialComponent>())
			drawMaterial(entity, state);

		if (entity.hasComponent<Fufu::CameraComponent>())
			drawCamera(entity);

		// Bouton "Add Component"
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Add Component", ImVec2(-1, 0)))
			ImGui::OpenPopup("AddComponentPopup");

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			if (!entity.hasComponent<Fufu::MeshComponent>()
				&& ImGui::MenuItem("Mesh"))
				entity.addComponent<Fufu::MeshComponent>();

			if (!entity.hasComponent<Fufu::MaterialComponent>()
				&& ImGui::MenuItem("Material"))
				entity.addComponent<Fufu::MaterialComponent>();

			if (!entity.hasComponent<Fufu::CameraComponent>()
				&& ImGui::MenuItem("Camera"))
				entity.addComponent<Fufu::CameraComponent>();

			ImGui::EndPopup();
		}

		ImGui::End();
	}

	// ----------------------------------------------------------------

	void InspectorPanel::drawTag(Fufu::Entity entity)
	{
		auto& tag = entity.getComponent<Fufu::TagComponent>().tag;

		char buf[256];
		strncpy_s(buf, tag.c_str(), sizeof(buf) - 1);

		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##tag", buf, sizeof(buf)))
			tag = std::string(buf);
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
		drawVec3Control("Rotation", t.rotation, 0.f, 0.01f);
		drawVec3Control("Scale", t.scale, 1.f, 0.01f);

		// Reset accumulation si la caméra principale a bougé
		bool changed = t.position != posBefore
			|| t.rotation != rotBefore
			|| t.scale != scaBefore;

		if (changed && entity.hasComponent<Fufu::CameraComponent>()
			&& entity.getComponent<Fufu::CameraComponent>().primary)
			Fufu::Application::get().getRenderer().resetAccumulation();
		else if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();
	}

	void InspectorPanel::drawMesh(Fufu::Entity entity)
	{
		if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& mesh = entity.getComponent<Fufu::MeshComponent>();

		char buf[512];
		strncpy_s(buf, mesh.meshPath.c_str(), sizeof(buf) - 1);

		ImGui::Text("Path");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::InputText("##meshpath", buf, sizeof(buf),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			mesh.meshPath = std::string(buf);
			mesh.meshID = 0; // invalider le cache UUID
			Fufu::Application::get().getRenderer().resetAccumulation();
		}
		ImGui::PopItemWidth();

		ImGui::TextDisabled("UUID: %llu", mesh.meshID);

		// Bouton Remove
		if (ImGui::SmallButton("Remove##mesh"))
			entity.removeComponent<Fufu::MeshComponent>();
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

		if (ImGui::SliderFloat("Metallic##mat", &mat.metallic, 0.f, 1.f)) changed = true;
		if (ImGui::SliderFloat("Roughness##mat", &mat.roughness, 0.f, 1.f)) changed = true;
		if (ImGui::SliderFloat("Emissive##mat", &mat.emissive, 0.f, 20.f)) changed = true;

		if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();

		if (ImGui::SmallButton("Remove##mat"))
			entity.removeComponent<Fufu::MaterialComponent>();
	}

	void InspectorPanel::drawCamera(Fufu::Entity entity)
	{
		if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& cam = entity.getComponent<Fufu::CameraComponent>();
		bool changed = false;

		// Primary toggle
		if (ImGui::Checkbox("Primary", &cam.primary)) changed = true;

		// Projection
		int proj = static_cast<int>(cam.projection);
		if (ImGui::RadioButton("Perspective", &proj, 0))
		{
			cam.projection = Fufu::CameraProjection::Perspective;  changed = true;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Orthographic", &proj, 1))
		{
			cam.projection = Fufu::CameraProjection::Orthographic; changed = true;
		}

		if (cam.projection == Fufu::CameraProjection::Perspective)
		{
			float fovDeg = glm::degrees(cam.fov);
			if (ImGui::SliderFloat("FOV", &fovDeg, 10.f, 120.f))
			{
				cam.fov = glm::radians(fovDeg); changed = true;
			}
		}
		else
		{
			if (ImGui::SliderFloat("Ortho Size", &cam.orthoSize, 0.1f, 100.f))
				changed = true;
		}

		if (ImGui::DragFloat("Near", &cam.nearPlane, 0.01f, 0.001f, 10.f))  changed = true;
		if (ImGui::DragFloat("Far", &cam.farPlane, 1.f, 1.f, 10000.f)) changed = true;

		if (changed)
			Fufu::Application::get().getRenderer().resetAccumulation();

		if (ImGui::SmallButton("Remove##cam"))
			entity.removeComponent<Fufu::CameraComponent>();
	}

	template<typename T>
	void InspectorPanel::drawAddComponentButton(Fufu::Entity entity, const char* label)
	{
		if (!entity.hasComponent<T>() && ImGui::MenuItem(label))
			entity.addComponent<T>();
	}

}