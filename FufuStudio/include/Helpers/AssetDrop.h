#pragma once

#include <Project/Assets/Asset.h>
#include <optional>

namespace FufuStudio
{
	// Résout le payload ImGui "ASSET_UUID" (déposé depuis l'onglet Assets du
	// ProjectPanel) en AssetMeta. Renvoie nullopt si aucun drop de ce type
	// n'est en cours, ou si l'UUID est inconnu de l'AssetManager du projet actif.
	// Doit être appelée entre ImGui::BeginDragDropTarget()/EndDragDropTarget().
	std::optional<Fufu::AssetMeta> acceptAssetDrop();
}
