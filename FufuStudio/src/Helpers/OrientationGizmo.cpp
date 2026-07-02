#include "Helpers/OrientationGizmo.h"
#include <Project/Components.h>
#include <imgui.h>
#include <array>
#include <algorithm>

namespace FufuStudio
{
	void OrientationGizmo::render(EditorState& state)
	{
		auto scene = state.getActiveScene();
		if (!scene) return;

		Fufu::Entity cam = scene->getPrimaryCamera();
		if (!cam) return;

		auto& camTransform = cam.getComponent<Fufu::TransformComponent>();
		glm::mat4 view = glm::inverse(camTransform.getTransform());
		glm::mat3 viewRot = glm::mat3(view);

		const float radius = 32.f;
		ImVec2 center = ImVec2(
			state.viewportPos.x + state.viewportSize.x - (radius + 24.f),
			state.viewportPos.y + radius + 24.f
		);

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddCircleFilled(center, radius + 16.f, IM_COL32(20, 20, 22, 90));

		struct Axis { glm::vec3 dir; ImU32 color; const char* label; };
		const Axis axes[3] = {
			{ {1.f, 0.f, 0.f}, IM_COL32(220, 65, 65, 255),  "X" },
			{ {0.f, 1.f, 0.f}, IM_COL32(70, 190, 70, 255),  "Y" },
			{ {0.f, 0.f, 1.f}, IM_COL32(70, 120, 230, 255), "Z" },
		};

		struct Endpoint { ImVec2 pos; ImU32 color; const char* label; bool positive; float depth; glm::vec3 axisDir; };
		std::array<Endpoint, 6> endpoints{};
		std::size_t idx = 0;

		for (const Axis& axis : axes)
		{
			glm::vec3 viewDir = viewRot * axis.dir;
			ImVec2 offset = ImVec2(viewDir.x * radius, -viewDir.y * radius);

			endpoints[idx++] = { ImVec2(center.x + offset.x, center.y + offset.y), axis.color, axis.label, true, viewDir.z, axis.dir };
			endpoints[idx++] = { ImVec2(center.x - offset.x, center.y - offset.y), axis.color, axis.label, false, -viewDir.z, -axis.dir };
		}

		// Dessine les extrémités les plus lointaines en premier (peintre simple)
		std::sort(endpoints.begin(), endpoints.end(),
			[](const Endpoint& a, const Endpoint& b) { return a.depth < b.depth; });

		bool windowHovered = ImGui::IsWindowHovered();
		ImVec2 mouse = ImGui::GetMousePos();
		const float hitRadius = 10.f;

		for (const Endpoint& e : endpoints)
		{
			drawList->AddLine(center, e.pos, e.positive ? e.color : IM_COL32(130, 130, 130, 160), 2.f);

			float dx = mouse.x - e.pos.x;
			float dy = mouse.y - e.pos.y;
			bool hovered = windowHovered && (dx * dx + dy * dy) <= hitRadius * hitRadius;

			if (e.positive)
			{
				drawList->AddCircleFilled(e.pos, hovered ? 10.f : 8.f, e.color);
				ImVec2 textSize = ImGui::CalcTextSize(e.label);
				drawList->AddText(ImVec2(e.pos.x - textSize.x * 0.5f, e.pos.y - textSize.y * 0.5f),
					IM_COL32(255, 255, 255, 255), e.label);
			}
			else
			{
				drawList->AddCircle(e.pos, hovered ? 9.f : 7.f, IM_COL32(190, 190, 190, 200), 12, 1.5f);
			}

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				snapTo(state, e.axisDir);
		}
	}

	void OrientationGizmo::handleShortcuts(EditorState& state)
	{
		if (!state.viewportFocused || !state.viewportHovered)
			return;

		bool ctrl = ImGui::GetIO().KeyCtrl;

		// Convention (moteur Y-up) : Front = +Z, Right = +X, Top = +Y — Ctrl
		// pour la vue opposée (Back/Left/Bottom), comme Blender.
		if (ImGui::IsKeyPressed(ImGuiKey_Keypad1)) snapTo(state, ctrl ? glm::vec3(0.f, 0.f, -1.f) : glm::vec3(0.f, 0.f, 1.f));
		if (ImGui::IsKeyPressed(ImGuiKey_Keypad3)) snapTo(state, ctrl ? glm::vec3(-1.f, 0.f, 0.f) : glm::vec3(1.f, 0.f, 0.f));
		if (ImGui::IsKeyPressed(ImGuiKey_Keypad7)) snapTo(state, ctrl ? glm::vec3(0.f, -1.f, 0.f) : glm::vec3(0.f, 1.f, 0.f));
	}

	void OrientationGizmo::snapTo(EditorState& state, const glm::vec3& axisDir)
	{
		// Orbite autour de l'entité sélectionnée, sinon l'origine.
		glm::vec3 pivot(0.f);
		Fufu::Entity primary = state.selection.primary();
		if (primary && primary.isValid() && primary.hasComponent<Fufu::TransformComponent>())
			pivot = primary.getComponent<Fufu::TransformComponent>().position;

		float distance = glm::length(state.cameraPosition - pivot);
		if (distance < 0.5f) distance = 5.f;

		glm::vec3 dir = glm::normalize(axisDir);
		state.cameraPosition = pivot + dir * distance;

		// Même dérivation que le forward de la caméra (voir ViewportPanel) :
		// pitch/yaw à partir du vecteur "regard" souhaité.
		glm::vec3 forward = -dir;
		float pitch = glm::clamp(glm::asin(glm::clamp(forward.y, -1.f, 1.f)), glm::radians(-89.f), glm::radians(89.f));
		float yaw = glm::atan(-forward.x, -forward.z);

		state.cameraRotation.x = pitch;
		state.cameraRotation.y = yaw;

		m_Renderer.resetAccumulation();
	}
}
