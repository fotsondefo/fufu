#include "Helpers/AssetDrop.h"
#include <Application/Application.h>
#include <imgui.h>

namespace FufuStudio
{
	std::optional<Fufu::AssetMeta> acceptAssetDrop()
	{
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_UUID");
		if (!payload)
			return std::nullopt;

		uint64_t uuidVal = *static_cast<const uint64_t*>(payload->Data);

		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject())
			return std::nullopt;

		auto& pool = pm.getCurrentProject().getAssetManager().getPool();
		auto it = pool.find(Fufu::UUID(uuidVal));
		if (it == pool.end())
			return std::nullopt;

		return it->second->getMeta();
	}
}
