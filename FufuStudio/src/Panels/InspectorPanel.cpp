#include "Panels/InspectorPanel.h"
#include <Project/Components.h>
#include <Project/Assets/TextureAsset.h>
#include <Project/WorldSettings.h>
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

	// Helper: draws a left-aligned label + widget on the right
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

		// Double-click to reset
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

		if (entity.hasComponent<Fufu::SubMeshMaterialsComponent>())
			drawSubMeshMaterials(entity, state);
		else if (entity.hasComponent<Fufu::MaterialComponent>())
			drawMaterial(entity, state);

		if (entity.hasComponent<Fufu::CameraComponent>())
			drawCamera(entity, state);

		if (entity.hasComponent<Fufu::GroomComponent>())
			drawGroom(entity, state);

		if (entity.hasComponent<Fufu::LightComponent>())
			drawLight(entity, state);

		if (entity.hasComponent<Fufu::AnimatorComponent>())
			drawAnimator(entity, state);

		if (entity.hasComponent<Fufu::VolumeComponent>())
			drawVolume(entity, state);

		if (entity.hasComponent<Fufu::GaussianSplatComponent>())
			drawGaussianSplat(entity, state);

		// "Add Component" button
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Add Component", ImVec2(-1, 0)))
			ImGui::OpenPopup("AddComponentPopup");

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			drawAddComponentButton<Fufu::MeshComponent>(entity, "Mesh", state);
			drawAddComponentButton<Fufu::MaterialComponent>(entity, "Material", state);
			drawAddComponentButton<Fufu::CameraComponent>(entity, "Camera", state);

			// Groom needs a Mesh (host surface) to produce anything
			if (entity.hasComponent<Fufu::MeshComponent>())
				drawAddComponentButton<Fufu::GroomComponent>(entity, "Groom", state);

			drawAddComponentButton<Fufu::LightComponent>(entity, "Light", state);
			drawAddComponentButton<Fufu::VolumeComponent>(entity, "Volume", state);
			drawAddComponentButton<Fufu::GaussianSplatComponent>(entity, "Gaussian Splat", state);

			if (entity.hasComponent<Fufu::MeshComponent>())
				drawAddComponentButton<Fufu::AnimatorComponent>(entity, "Animator", state);

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

		// Fetch world settings for unit conversion (default = meters)
		Fufu::WorldSettings ws;
		{
			auto& pm = Fufu::Application::get().getProjectManager();
			if (pm.hasProject()) ws = pm.getCurrentProject().getWorldSettings();
		}
		const float upm = ws.unitsPerMeter();

		glm::vec3 posBefore = t.position;
		glm::vec3 rotBefore = t.rotation;
		glm::vec3 scaBefore = t.scale;

		// Position: display in chosen unit, store in meters
		{
			glm::vec3 disp = t.position * upm;
			std::string label = std::string("Position (") + ws.suffix() + ")";
			drawVec3Control(label.c_str(), disp, 0.f, ws.dragStep());
			trackEdit(entity, m_PendingTransform, state);
			t.position = disp / upm;
		}

		// Rotation: display in degrees, store in radians
		{
			glm::vec3 deg = glm::degrees(t.rotation);
			drawVec3Control("Rotation (deg)", deg, 0.f, 0.5f);
			trackEdit(entity, m_PendingTransform, state);
			t.rotation = glm::radians(deg);
		}

		drawVec3Control("Scale", t.scale, 1.f, 0.01f);
		trackEdit(entity, m_PendingTransform, state);

		// Reset accumulation if the primary camera has moved
		bool changed = t.position != posBefore || t.rotation != rotBefore || t.scale != scaBefore;

		if (changed)
		{
			// A camera never goes through the GPUScene (its transform is read
			// every frame directly by the Renderer): only an entity with
			// geometry needs to mark the scene dirty for re-upload.
			if (!entity.hasComponent<Fufu::CameraComponent>())
				if (auto* scene = entity.getScene())
					scene->markDirty();

			Fufu::Application::get().getRenderer().resetAccumulation();
		}
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
			if (auto* scene = entity.getScene()) scene->markDirty();
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

		// Albedo with color picker
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
		if (ImGui::SliderFloat("IOR##mat", &mat.ior, 1.0f, 3.0f, "%.3f")) changed = true;
		trackEdit(entity, m_PendingMaterial, state);
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Index of Refraction: 1.0=air, 1.33=water, 1.5=glass, 2.4=diamond");

		auto& pm = Fufu::Application::get().getProjectManager();
		auto resolveTexName = [&](uint64_t id) -> std::string {
			if (id == 0) return "(none)";
			if (pm.hasProject())
			{
				if (auto tex = pm.getCurrentProject().getAssetManager().getAsset<Fufu::TextureAsset>(Fufu::UUID(id)))
					return tex->getMeta().sourcePath.filename().string();
				return "(missing)";
			}
			return "(no project)";
		};

		// ── Albedo texture ────────────────────────────────────────────────────
		ImGui::Spacing();
		ImGui::Text("Albedo Texture");
		ImGui::TextDisabled("%s", resolveTexName(mat.albedoTexID).c_str());
		ImGui::Button(ICON_FA_FILE_IMAGE_O " Drop albedo texture##albedotex", ImVec2(-1, 26.f));
		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Texture)
			{
				mat.albedoTexID = meta->uuid.value();
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}
		if (mat.albedoTexID != 0 && ImGui::SmallButton("Clear##albedotex"))
		{
			mat.albedoTexID = 0;
			changed = true;
		}

		// ── Normal map ────────────────────────────────────────────────────────
		ImGui::Spacing();
		ImGui::Text("Normal Map");
		ImGui::TextDisabled("%s", resolveTexName(mat.normalTexID).c_str());
		ImGui::Button(ICON_FA_FILE_IMAGE_O " Drop normal map##normaltex", ImVec2(-1, 26.f));
		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Texture)
			{
				mat.normalTexID = meta->uuid.value();
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}
		if (mat.normalTexID != 0 && ImGui::SmallButton("Clear##normaltex"))
		{
			mat.normalTexID = 0;
			changed = true;
		}

		// ── ORM texture (R=AO, G=Roughness, B=Metallic) ───────────────────────
		ImGui::Spacing();
		ImGui::Text("ORM Texture");
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Packed: R=Ambient Occlusion, G=Roughness, B=Metallic\nOverrides the sliders above when assigned.");
		ImGui::TextDisabled("%s", resolveTexName(mat.ormTexID).c_str());
		ImGui::Button(ICON_FA_FILE_IMAGE_O " Drop ORM texture##ormtex", ImVec2(-1, 26.f));
		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Texture)
			{
				mat.ormTexID = meta->uuid.value();
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}
		if (mat.ormTexID != 0 && ImGui::SmallButton("Clear##ormtex"))
		{
			mat.ormTexID = 0;
			changed = true;
		}

		if (changed)
		{
			if (auto* scene = entity.getScene()) scene->markDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		// Offer per-submesh upgrade if the mesh has multiple submeshes
		if (entity.hasComponent<Fufu::MeshComponent>())
		{
			auto& meshComp = entity.getComponent<Fufu::MeshComponent>();
			auto& am = Fufu::Application::get().getProjectManager().getCurrentProject().getAssetManager();
			if (auto mesh = am.getMesh(meshComp.meshPath); mesh && mesh->getSubMeshCount() > 1)
			{
				ImGui::Spacing();
				ImGui::TextDisabled("Mesh has %d submeshes", (int)mesh->getSubMeshCount());
				if (ImGui::Button(ICON_FA_CUBE " Enable Per-Submesh Materials"))
				{
					Fufu::SubMeshMaterialsComponent smm;
					smm.slots.resize(mesh->getSubMeshCount(), mat); // copy current mat to all slots
					entity.addComponent<Fufu::SubMeshMaterialsComponent>(std::move(smm));
					if (auto* scene = entity.getScene()) scene->markDirty();
					Fufu::Application::get().getRenderer().resetAccumulation();
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Assign a different material to each mesh section.");
			}
		}

		if (ImGui::SmallButton("Remove##mat"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::MaterialComponent>>(entity);
	}

	// ── Per-submesh material slot (shared helper) ──────────────────────────────

	bool InspectorPanel::drawOneMaterialSlot(Fufu::MaterialComponent& mat, int slotIdx, EditorState& /*state*/)
	{
		bool changed = false;
		ImGui::PushID(slotIdx);

		auto& pm = Fufu::Application::get().getProjectManager();
		auto resolveTexName = [&](uint64_t id) -> std::string {
			if (id == 0) return "(none)";
			if (pm.hasProject())
			{
				if (auto tex = pm.getCurrentProject().getAssetManager().getAsset<Fufu::TextureAsset>(Fufu::UUID(id)))
					return tex->getMeta().sourcePath.filename().string();
				return "(missing)";
			}
			return "(no project)";
		};

		float col[4] = { mat.albedo.r, mat.albedo.g, mat.albedo.b, mat.albedo.a };
		ImGui::Text("Albedo"); ImGui::SameLine();
		if (ImGui::ColorEdit4("##albedo", col))
		{
			mat.albedo = glm::vec4(col[0], col[1], col[2], col[3]);
			changed = true;
		}
		if (ImGui::SliderFloat("Metallic##m",  &mat.metallic,  0.f, 1.f))  changed = true;
		if (ImGui::SliderFloat("Roughness##r", &mat.roughness, 0.f, 1.f))  changed = true;
		if (ImGui::SliderFloat("Emissive##e",  &mat.emissive,  0.f, 20.f)) changed = true;
		if (ImGui::SliderFloat("IOR##i",       &mat.ior,       1.f, 3.f, "%.3f")) changed = true;

		// Albedo texture
		ImGui::TextDisabled("%s", resolveTexName(mat.albedoTexID).c_str());
		ImGui::Button(ICON_FA_FILE_IMAGE_O " Drop albedo##a", ImVec2(-1, 22.f));
		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Texture)
			{ mat.albedoTexID = meta->uuid.value(); changed = true; }
			ImGui::EndDragDropTarget();
		}
		if (mat.albedoTexID && ImGui::SmallButton("Clear##ca")) { mat.albedoTexID = 0; changed = true; }

		// Normal map
		ImGui::TextDisabled("%s", resolveTexName(mat.normalTexID).c_str());
		ImGui::Button(ICON_FA_FILE_IMAGE_O " Drop normal##n", ImVec2(-1, 22.f));
		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Texture)
			{ mat.normalTexID = meta->uuid.value(); changed = true; }
			ImGui::EndDragDropTarget();
		}
		if (mat.normalTexID && ImGui::SmallButton("Clear##cn")) { mat.normalTexID = 0; changed = true; }

		// ORM
		ImGui::TextDisabled("%s", resolveTexName(mat.ormTexID).c_str());
		ImGui::Button(ICON_FA_FILE_IMAGE_O " Drop ORM##o", ImVec2(-1, 22.f));
		if (ImGui::BeginDragDropTarget())
		{
			if (auto meta = acceptAssetDrop(); meta && meta->type == Fufu::AssetType::Texture)
			{ mat.ormTexID = meta->uuid.value(); changed = true; }
			ImGui::EndDragDropTarget();
		}
		if (mat.ormTexID && ImGui::SmallButton("Clear##co")) { mat.ormTexID = 0; changed = true; }

		ImGui::PopID();
		return changed;
	}

	void InspectorPanel::drawSubMeshMaterials(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Material Slots", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& smm = entity.getComponent<Fufu::SubMeshMaterialsComponent>();

		// Determine slot names from the mesh submesh names
		std::vector<std::string> subNames;
		if (entity.hasComponent<Fufu::MeshComponent>())
		{
			auto& am = Fufu::Application::get().getProjectManager().getCurrentProject().getAssetManager();
			if (auto mesh = am.getMesh(entity.getComponent<Fufu::MeshComponent>().meshPath))
			{
				for (const auto& sub : mesh->getSubMeshes())
					subNames.push_back(sub.name.empty() ? "Unnamed" : sub.name);
			}
		}

		bool changed = false;
		int n = static_cast<int>(smm.slots.size());
		for (int i = 0; i < n; ++i)
		{
			std::string label = (i < (int)subNames.size())
				? subNames[i] : ("Slot " + std::to_string(i));
			std::string header = ICON_FA_CUBE " " + label + "##slothdr" + std::to_string(i);

			if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (drawOneMaterialSlot(smm.slots[i], i, state))
					changed = true;
				ImGui::TreePop();
			}
		}

		if (changed)
		{
			if (auto* scene = entity.getScene()) scene->markDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		// Option to revert to single material
		ImGui::Spacing();
		if (ImGui::SmallButton("Revert to Single Material"))
		{
			// Copy first slot back to MaterialComponent if it exists
			if (!smm.slots.empty() && entity.hasComponent<Fufu::MaterialComponent>())
				entity.getComponent<Fufu::MaterialComponent>() = smm.slots[0];
			entity.removeComponent<Fufu::SubMeshMaterialsComponent>();
			if (auto* scene = entity.getScene()) scene->markDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}
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
		{
			if (auto* scene = entity.getScene()) scene->markDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

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

		// Unit picker — changes drag range for sensible defaults per unit
		{
			static const char* unitNames[] = { "Arbitrary", "Lux (lm/m²)", "Lumens", "Candelas (lm/sr)" };
			int unitIdx = static_cast<int>(light.unit);
			if (ImGui::Combo("Unit##lightunit", &unitIdx, unitNames, 4))
			{
				light.unit = static_cast<Fufu::LightUnit>(unitIdx);
				changed = true;
			}
			trackEdit(entity, m_PendingLight, state);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(
					"Arbitrary: raw multiplier (default)\n"
					"Lux: illuminance at surface — sun ~100 000 lx\n"
					"Lumens: total flux — 60W bulb ~800 lm\n"
					"Candelas: intensity per steradian");
		}

		// Intensity drag — range adapts to the chosen unit
		float dragStep, dragMax;
		switch (light.unit)
		{
		case Fufu::LightUnit::Lux:     dragStep = 500.f;  dragMax = 150000.f; break;
		case Fufu::LightUnit::Lumen:   dragStep = 50.f;   dragMax = 50000.f;  break;
		case Fufu::LightUnit::Candela: dragStep = 50.f;   dragMax = 50000.f;  break;
		default:
			dragStep = (light.type == Fufu::LightType::Directional) ? 0.05f : 1.f;
			dragMax  = (light.type == Fufu::LightType::Directional) ? 100.f  : 10000.f;
			break;
		}
		changed |= ImGui::DragFloat("Intensity##lightint", &light.intensity, dragStep, 0.f, dragMax);
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
		{
			if (auto* scene = entity.getScene()) scene->markDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		if (ImGui::SmallButton("Remove##light"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::LightComponent>>(entity);
	}

	void InspectorPanel::drawAnimator(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& anim = entity.getComponent<Fufu::AnimatorComponent>();

		// Fetch clip list from the mesh asset
		std::vector<std::string> clipNames;
		bool hasBones = false;
		if (entity.hasComponent<Fufu::MeshComponent>())
		{
			auto& pm = Fufu::Application::get().getProjectManager();
			if (pm.hasProject())
			{
				auto& am = pm.getCurrentProject().getAssetManager();
				auto meshAsset = am.getMesh(entity.getComponent<Fufu::MeshComponent>().meshPath);
				if (meshAsset)
				{
					hasBones = meshAsset->hasBones();
					for (const auto& clip : meshAsset->getAnimationClips())
						clipNames.push_back(clip.name);
				}
			}
		}

		if (!hasBones)
		{
			ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.f),
				"Mesh has no skeleton / bone data.");
		}
		else if (clipNames.empty())
		{
			ImGui::TextDisabled("Mesh has a skeleton but no animation clips.");
		}
		else
		{
			// Clip selector
			const char* current = (anim.clipIndex >= 0 && anim.clipIndex < (int)clipNames.size())
				? clipNames[anim.clipIndex].c_str() : "(none)";
			if (ImGui::BeginCombo("Clip##anim", current))
			{
				for (int i = 0; i < (int)clipNames.size(); ++i)
				{
					bool sel = (i == anim.clipIndex);
					if (ImGui::Selectable(clipNames[i].c_str(), sel))
					{
						anim.clipIndex = i;
						anim.time = 0.f;
					}
					if (sel) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			// Playback controls
			ImGui::Checkbox("Playing", &anim.playing);
			ImGui::SameLine();
			ImGui::Checkbox("Loop",    &anim.loop);
			ImGui::SliderFloat("Speed##anim", &anim.speed, 0.f, 4.f, "%.2fx");

			if (ImGui::Button("Rewind")) { anim.time = 0.f; anim.currentBoneMatrices.clear(); }
			ImGui::SameLine();
			ImGui::Text("Time: %.2f s", anim.time);
		}

		ImGui::Spacing();
		if (ImGui::SmallButton("Remove##anim"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::AnimatorComponent>>(entity);
	}

	void InspectorPanel::drawVolume(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Volume", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& v = entity.getComponent<Fufu::VolumeComponent>();
		bool changed = false;

		ImGui::TextDisabled("Bounds: entity Position + Scale (m)");
		ImGui::Spacing();

		// Appearance
		float alb[3] = { v.albedo.r, v.albedo.g, v.albedo.b };
		if (ImGui::ColorEdit3("Albedo##vol", alb))
		{ v.albedo = { alb[0], alb[1], alb[2] }; changed = true; }
		trackEdit(entity, m_PendingVolume, state);

		float em[3] = { v.emission.r, v.emission.g, v.emission.b };
		if (ImGui::ColorEdit3("Emission##vol", em))
		{ v.emission = { em[0], em[1], em[2] }; changed = true; }
		trackEdit(entity, m_PendingVolume, state);

		if (ImGui::DragFloat("Emission Strength##vol", &v.emissionStrength, 0.1f, 0.f, 500.f))
			changed = true;
		trackEdit(entity, m_PendingVolume, state);

		ImGui::Separator();
		ImGui::Text("Extinction");

		if (ImGui::DragFloat("Density##vol",     &v.density,    0.01f, 0.001f, 100.f)) changed = true;
		trackEdit(entity, m_PendingVolume, state);
		if (ImGui::SliderFloat("Scattering##vol", &v.scattering, 0.f, 1.f)) changed = true;
		trackEdit(entity, m_PendingVolume, state);
		if (ImGui::SliderFloat("Absorption##vol", &v.absorption, 0.f, 1.f)) changed = true;
		trackEdit(entity, m_PendingVolume, state);
		if (ImGui::SliderFloat("Anisotropy (g)##vol", &v.anisotropy, -1.f, 1.f)) changed = true;
		trackEdit(entity, m_PendingVolume, state);

		ImGui::DragInt("March Steps##vol", &v.marchSteps, 1.f, 4, 256);
		trackEdit(entity, m_PendingVolume, state);

		ImGui::Separator();
		ImGui::Text("Density Field");

		static const char* noiseNames[] = { "Uniform (no noise)", "Value Noise", "FBM" };
		int noiseIdx = static_cast<int>(v.noiseType);
		if (ImGui::Combo("Noise##vol", &noiseIdx, noiseNames, 3))
		{
			v.noiseType = static_cast<Fufu::VolumeNoiseType>(noiseIdx);
			changed = true;
		}

		if (v.noiseType != Fufu::VolumeNoiseType::None)
		{
			if (ImGui::DragFloat("Scale##volnoise", &v.noiseScale, 0.1f, 0.1f, 32.f)) changed = true;
			trackEdit(entity, m_PendingVolume, state);
			if (v.noiseType == Fufu::VolumeNoiseType::FBM)
			{
				if (ImGui::DragInt("Octaves##volnoise",   &v.noiseOctaves,   1.f, 1, 8))   changed = true;
				trackEdit(entity, m_PendingVolume, state);
				if (ImGui::DragFloat("Lacunarity##volnoise", &v.noiseLacunarity, 0.05f, 1.f, 4.f)) changed = true;
				trackEdit(entity, m_PendingVolume, state);
				if (ImGui::DragFloat("Gain##volnoise",       &v.noiseGain,       0.05f, 0.1f, 0.9f)) changed = true;
				trackEdit(entity, m_PendingVolume, state);
			}
		}

		if (changed)
		{
			if (auto* scene = entity.getScene()) scene->markDirty();
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		ImGui::Spacing();
		if (ImGui::SmallButton("Remove##vol"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::VolumeComponent>>(entity);
	}

	void InspectorPanel::drawGaussianSplat(Fufu::Entity entity, EditorState& state)
	{
		if (!ImGui::CollapsingHeader("Gaussian Splat", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& gs = entity.getComponent<Fufu::GaussianSplatComponent>();

		// Path
		char buf[512];
		strncpy_s(buf, gs.path.c_str(), sizeof(buf) - 1);
		ImGui::Text("PLY File");
		ImGui::SameLine();
		ImGui::PushItemWidth(-60.f);
		if (ImGui::InputText("##gspath", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			gs.path = std::string(buf);
			Fufu::Application::get().getRenderer().resetAccumulation();
		}
		trackEdit(entity, m_PendingGS, state);
		ImGui::PopItemWidth();

		// Browse button
		ImGui::SameLine();
		if (ImGui::Button("...##gs"))
		{
			NFD::Guard g;
			NFD::UniquePath p;
			nfdfilteritem_t filter = { "PLY Gaussian Splat", "ply" };
			if (NFD::OpenDialog(p, &filter, 1) == NFD_OKAY)
			{
				gs.path = p.get();
				Fufu::Application::get().getRenderer().resetAccumulation();
			}
		}

		// Appearance
		ImGui::Separator();
		if (ImGui::SliderFloat("Opacity##gs", &gs.opacity, 0.f, 1.f))
		{
			trackEdit(entity, m_PendingGS, state);
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		// SH degree
		static const char* shNames[] = { "DC only (degree 0)", "Degree 1", "Degree 2", "Degree 3 (full)" };
		int deg = std::clamp(gs.shDegree, 0, 3);
		if (ImGui::Combo("SH Degree##gs", &deg, shNames, 4))
		{
			gs.shDegree = deg;
			Fufu::Application::get().getRenderer().resetAccumulation();
		}

		ImGui::TextDisabled("Gaussians are transformed by the entity's Transform.");

		ImGui::Spacing();
		if (ImGui::SmallButton("Remove##gs"))
			state.commandHistory->executeCommand<ComponentRemoveCommand<Fufu::GaussianSplatComponent>>(entity);
	}

}