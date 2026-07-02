#include "Panels/HierarchyPanel.h"
#include <Project/Components.h>
#include <imgui.h>
#include "Helpers/FontIcons.h"
#include "Commands/CommandHistory.h"
#include "Commands/CompositeCommand.h"
#include "Commands/EntityCommands.h"

namespace FufuStudio 
{
	void HierarchyPanel::onImGuiRender(EditorState& state)
	{
		ImGui::Begin(ICON_FA_BAR_CHART " Hierarchy##hierarchy");

		std::shared_ptr<Fufu::Scene> activeScene = state.getActiveScene();

		if (!activeScene)
		{
			ImGui::TextDisabled("No active scene");
			ImGui::End();
			return;
		}

		// Create Entity button
		if (ImGui::Button("+ Entity"))
			state.commandHistory->executeCommand<EntityCreateCommand>(activeScene, "New Entity");

		ImGui::Separator();

		// Display entities
		auto& reg = activeScene->getRegistry();
		reg.each([&](entt::entity handle)
		{
			if (reg.all_of<Fufu::ParentComponent>(handle))
				return;

			Fufu::Entity entity(handle, activeScene.get());
			drawEntityNode(entity, state);
		});

		// Deselect if we clic on an empty area
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
			state.selection.clear();

		// Delete key : supprime toute la sélection courante
		if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && !state.selection.empty())
		{
			std::vector<Fufu::Entity> targets = state.selection.entities();
			state.selection.clear();
			deleteEntities(state, activeScene, targets);
		}

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

		if (state.selection.isSelected(entity))
			flags |= ImGuiTreeNodeFlags_Selected;

		bool hasChildren = entity.hasComponent<Fufu::ChildrenComponent>() && !entity.getComponent<Fufu::ChildrenComponent>().children.empty();

		if (!hasChildren)
			flags |= ImGuiTreeNodeFlags_Leaf;

		bool opened = ImGui::TreeNodeEx(
			reinterpret_cast<void*>(
				static_cast<uintptr_t>(
					static_cast<uint32_t>(entity.getHandle())
					)
				)
			, flags, "%s", tag.c_str()
		);

		if (ImGui::IsItemClicked())
		{
			if (ImGui::GetIO().KeyCtrl)
				state.selection.toggle(entity);
			else
				state.selection.select(entity);
		}

		std::shared_ptr<Fufu::Scene> activeScene = state.getActiveScene();

		// Contextual menu 
		bool deleted = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Create Child"))
				state.commandHistory->executeCommand<EntityCreateCommand>(activeScene, "Child Entity", entity);

			if (ImGui::MenuItem("Duplicate"))
				state.commandHistory->executeCommand<EntityDuplicateCommand>(activeScene, entity);
			
			ImGui::Separator();
			if (ImGui::MenuItem("Delete"))
				deleted = true;

			ImGui::EndPopup();
		}

		// Drag & drop to parent
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
				Fufu::Entity src(static_cast<entt::entity>(srcHandle), activeScene.get());
				state.commandHistory->executeCommand<EntityReparentCommand>(activeScene, src, entity);
			}
			ImGui::EndDragDropTarget();
		}

		if (opened)
		{
			if (hasChildren)
			{
				for (entt::entity childHandle : entity.getComponent<Fufu::ChildrenComponent>().children)
				{
					Fufu::Entity child(childHandle, activeScene.get());

					if (child.isValid())
						drawEntityNode(child, state);
				}
			}

			ImGui::TreePop();
		}

		// Delete after rendering to prevent the registry from being invalidated
		if (deleted)
		{
			// Si l'entité fait partie d'une multi-sélection, on supprime tout le
			// groupe (en un seul undo) ; sinon on ne touche qu'à cette entité.
			bool wasSelected = state.selection.isSelected(entity);
			std::vector<Fufu::Entity> targets = (wasSelected && state.selection.size() > 1)
				? state.selection.entities()
				: std::vector<Fufu::Entity>{ entity };

			if (wasSelected)
				state.selection.clear();

			deleteEntities(state, activeScene, targets);
		}
	}

	void HierarchyPanel::deleteEntities(EditorState& state, std::shared_ptr<Fufu::Scene> scene,
		const std::vector<Fufu::Entity>& targets)
	{
		if (targets.empty()) return;

		if (targets.size() == 1)
		{
			state.commandHistory->executeCommand<EntityDestroyCommand>(scene, targets.front());
			return;
		}

		auto composite = std::make_unique<CompositeCommand>("Delete Entities");
		for (Fufu::Entity e : targets)
		{
			if (e.isValid())
				composite->add(std::make_unique<EntityDestroyCommand>(scene, e));
		}

		if (!composite->empty())
			state.commandHistory->execute(std::move(composite));
	}

	void HierarchyPanel::drawContextMenu(EditorState& state)
	{
		std::shared_ptr<Fufu::Scene> activeScene = state.getActiveScene();

		if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
				state.commandHistory->executeCommand<EntityCreateCommand>(activeScene, "Empty Entity");

			if (ImGui::MenuItem("Create Camera"))
			{
				state.commandHistory->executeCommand<EntityCreateCommand>(activeScene, "Camera", Fufu::Entity{},
					[](Fufu::Entity e) { e.addComponent<Fufu::CameraComponent>(); });
			}

			ImGui::EndPopup();
		}
	}

}