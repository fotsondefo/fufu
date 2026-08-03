#pragma once

#include "IEditorPanel.h"
#include <Project/Components.h>
#include <optional>

namespace FufuStudio
{

	class InspectorPanel : public IEditorPanel
	{
	public:
		void onImGuiRender(EditorState& state) override;

	private:
		void drawTag(Fufu::Entity entity, EditorState& state);
		void drawTransform(Fufu::Entity entity, EditorState& state);
		void drawMesh(Fufu::Entity entity, EditorState& state);
		void drawMaterial(Fufu::Entity entity, EditorState& state);
		void drawSubMeshMaterials(Fufu::Entity entity, EditorState& state);
		bool drawOneMaterialSlot(Fufu::MaterialComponent& mat, int slotIdx, EditorState& state);
		void drawCamera(Fufu::Entity entity, EditorState& state);
		void drawGroom(Fufu::Entity entity, EditorState& state);
		void drawLight(Fufu::Entity entity, EditorState& state);
		void drawAnimator(Fufu::Entity entity, EditorState& state);
		void drawVolume(Fufu::Entity entity, EditorState& state);
		void drawGaussianSplat(Fufu::Entity entity, EditorState& state);

		template<typename T>
		void drawAddComponentButton(Fufu::Entity entity, const char* label, EditorState& state);

		// Pushes a ComponentEditCommand<Component> when an ImGui interaction
		// (drag/slider/input) ends on a widget belonging to this component.
		// Call right after the relevant widget(s) (see drawTransform).
		template<typename Component>
		void trackEdit(Fufu::Entity entity, std::optional<Component>& pending, EditorState& state);

	private:
		std::optional<Fufu::TagComponent>       m_PendingTag;
		std::optional<Fufu::TransformComponent> m_PendingTransform;
		std::optional<Fufu::MeshComponent>      m_PendingMesh;
		std::optional<Fufu::MaterialComponent>  m_PendingMaterial;
		std::optional<Fufu::CameraComponent>    m_PendingCamera;
		std::optional<Fufu::GroomComponent>     m_PendingGroom;
		std::optional<Fufu::LightComponent>     m_PendingLight;
		std::optional<Fufu::VolumeComponent>          m_PendingVolume;
		std::optional<Fufu::GaussianSplatComponent>   m_PendingGS;
	};

}