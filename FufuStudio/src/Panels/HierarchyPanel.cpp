#include "Panels/HierarchyPanel.h"
#include <Project/Components.h>
#include <imgui.h>

namespace FufuStudio 
{
	void HierarchyPanel::onImGuiRender(EditorState& state)
	{
		ImGui::Begin("Hierarchy");

		if (!state.activeScene)
		{
			ImGui::TextDisabled("No active scene");
			ImGui::End();
			return;
		}

		// Bouton création d'entité
		if (ImGui::Button("+ Entity"))
			state.activeScene->createEntity("New Entity");

		ImGui::Separator();

		// N'afficher que les entités racines (sans parent)
		auto& reg = state.activeScene->getRegistry();
		reg.each([&](entt::entity handle)
		{
			// Ignorer les entités qui ont un parent — elles seront
			// dessinées récursivement depuis leur racine
			if (reg.all_of<Fufu::ParentComponent>(handle))
				return;

			Fufu::Entity entity(handle, state.activeScene.get());
			drawEntityNode(entity, state);
		});

		// Clic dans le vide ? désélectionner
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)
			&& !ImGui::IsAnyItemHovered())
			state.selectedEntity = Fufu::Entity{};

		drawContextMenu(state);

		ImGui::End();
	}

	void HierarchyPanel::drawEntityNode(Fufu::Entity entity, EditorState& state)
	{
		auto& tag = entity.getComponent<Fufu::TagComponent>().tag;

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_OpenOnDoubleClick |
			ImGuiTreeNodeFlags_SpanAvailWidth;

		if (state.selectedEntity == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		// Feuille si pas d'enfants
		bool hasChildren = entity.hasComponent<Fufu::ChildrenComponent>()
			&& !entity.getComponent<Fufu::ChildrenComponent>().children.empty();

		if (!hasChildren)
			flags |= ImGuiTreeNodeFlags_Leaf;

		bool opened = ImGui::TreeNodeEx(
			reinterpret_cast<void*>(static_cast<uintptr_t>(
				static_cast<uint32_t>(entity.getHandle()))),
			flags,
			"%s", tag.c_str()
		);

		// Sélection au clic
		if (ImGui::IsItemClicked())
			state.selectedEntity = entity;

		// Menu contextuel clic droit sur une entité
		bool deleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Create Child"))
			{
				auto child = state.activeScene->createEntity("Child Entity");
				state.activeScene->setParent(child, entity);
			}
			if (ImGui::MenuItem("Duplicate"))
			{
				// Duplication simple — copie Tag et Transform
				auto dup = state.activeScene->createEntity(tag + " (copy)");
				auto& srcT = entity.getComponent<Fufu::TransformComponent>();
				auto& dstT = dup.getComponent<Fufu::TransformComponent>();
				dstT = srcT;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete"))
				deleted = true;

			ImGui::EndPopup();
		}

		// Drag & drop pour reparenter
		if (ImGui::BeginDragDropSource())
		{
			uint32_t handle = static_cast<uint32_t>(entity.getHandle());
			ImGui::SetDragDropPayload("ENTITY_ID", &handle, sizeof(uint32_t));
			ImGui::Text("Move: %s", tag.c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("ENTITY_ID"))
			{
				uint32_t srcHandle = *static_cast<const uint32_t*>(payload->Data);
				Fufu::Entity src(static_cast<entt::entity>(srcHandle),
					state.activeScene.get());
				state.activeScene->setParent(src, entity);
			}
			ImGui::EndDragDropTarget();
		}

		if (opened)
		{
			if (hasChildren)
			{
				for (entt::entity childHandle :
				entity.getComponent<Fufu::ChildrenComponent>().children)
				{
					Fufu::Entity child(childHandle, state.activeScene.get());
					if (child.isValid())
						drawEntityNode(child, state);
				}
			}
			ImGui::TreePop();
		}

		// Suppression après le rendu pour éviter l'invalidation du registre
		if (deleted)
		{
			if (state.selectedEntity == entity)
				state.selectedEntity = Fufu::Entity{};
			state.activeScene->destroyEntity(entity);
		}
	}

	void HierarchyPanel::drawContextMenu(EditorState& state)
	{
		// Clic droit dans le vide de la fenêtre
		if (ImGui::BeginPopupContextWindow("HierarchyContextMenu",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
				state.activeScene->createEntity("Empty Entity");
			if (ImGui::MenuItem("Create Camera"))
			{
				auto cam = state.activeScene->createEntity("Camera");
				auto& c = cam.addComponent<Fufu::CameraComponent>();
				c.primary = false;
			}
			ImGui::EndPopup();
		}
	}

}