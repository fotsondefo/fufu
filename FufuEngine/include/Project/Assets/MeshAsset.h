#pragma once

#include "Asset.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Fufu 
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 tangent;
	};

	struct SubMesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::string name;
	};

	class MeshAsset : public Asset
	{
	public:
		AssetType getType() const override { return AssetType::Mesh; }

		const std::vector<SubMesh>& getSubMeshes() const { return m_SubMeshes; }
		size_t getSubMeshCount() const { return m_SubMeshes.size(); }

		// Accès mutable pour les outils d'édition de mesh (voir ModelingTool
		// côté éditeur). Le Renderer re-upload la scène à chaque frame, donc
		// toute modification ici est visible immédiatement sans étape supplémentaire.
		std::vector<SubMesh>& getSubMeshesMutable() { return m_SubMeshes; }

	private:
		std::vector<SubMesh> m_SubMeshes;
		friend class AssetManager;
	};
}