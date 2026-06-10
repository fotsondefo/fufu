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

	private:
		std::vector<SubMesh> m_SubMeshes;
		friend class AssetManager;
	};
}