#include "depch.h"
#include "Project/Assets/MeshSimplifier.h"
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace Fufu
{
	namespace
	{
		struct CellKey
		{
			int x, y, z;
			bool operator==(const CellKey& other) const
			{
				return x == other.x && y == other.y && z == other.z;
			}
		};

		struct CellKeyHash
		{
			std::size_t operator()(const CellKey& k) const
			{
				std::size_t h = static_cast<std::size_t>(k.x) * 73856093u;
				h ^= static_cast<std::size_t>(k.y) * 19349663u;
				h ^= static_cast<std::size_t>(k.z) * 83492791u;
				return h;
			}
		};

		struct Accum
		{
			glm::vec3 position{ 0.f, 0.f, 0.f };
			glm::vec3 normal{ 0.f, 0.f, 0.f };
			glm::vec2 uv{ 0.f, 0.f };
			int count = 0;
			uint32_t newIndex = 0;
		};
	}

	SubMesh MeshSimplifier::simplify(const SubMesh& source, float cellSize)
	{
		if (cellSize <= 0.f || source.vertices.empty() || source.indices.empty())
			return source;

		std::unordered_map<CellKey, Accum, CellKeyHash> clusters;
		std::vector<uint32_t> vertexRemap(source.vertices.size());

		auto cellOf = [&](const glm::vec3& p)
		{
			return CellKey{
				static_cast<int>(glm::floor(p.x / cellSize)),
				static_cast<int>(glm::floor(p.y / cellSize)),
				static_cast<int>(glm::floor(p.z / cellSize))
			};
		};

		for (std::size_t i = 0; i < source.vertices.size(); ++i)
		{
			const Vertex& v = source.vertices[i];
			CellKey key = cellOf(v.position);

			auto it = clusters.find(key);
			if (it == clusters.end())
			{
				Accum acc;
				acc.position = v.position;
				acc.normal = v.normal;
				acc.uv = v.uv;
				acc.count = 1;
				acc.newIndex = static_cast<uint32_t>(clusters.size());
				it = clusters.emplace(key, acc).first;
			}
			else
			{
				it->second.position += v.position;
				it->second.normal += v.normal;
				it->second.uv += v.uv;
				it->second.count += 1;
			}

			vertexRemap[i] = it->second.newIndex;
		}

		SubMesh result;
		result.name = source.name;
		result.vertices.resize(clusters.size());

		for (const auto& [key, acc] : clusters)
		{
			Vertex v;
			v.position = acc.position / static_cast<float>(acc.count);
			v.normal = glm::length(acc.normal) > 1e-8f ? glm::normalize(acc.normal) : glm::vec3(0.f, 1.f, 0.f);
			v.uv = acc.uv / static_cast<float>(acc.count);
			v.tangent = glm::vec3(0.f, 0.f, 0.f);
			result.vertices[acc.newIndex] = v;
		}

		result.indices.reserve(source.indices.size());
		for (std::size_t i = 0; i + 2 < source.indices.size(); i += 3)
		{
			uint32_t a = vertexRemap[source.indices[i]];
			uint32_t b = vertexRemap[source.indices[i + 1]];
			uint32_t c = vertexRemap[source.indices[i + 2]];

			if (a == b || b == c || a == c)
				continue; // triangle dégénéré après fusion des sommets

			result.indices.push_back(a);
			result.indices.push_back(b);
			result.indices.push_back(c);
		}

		// Filet de sécurité : si le clustering a tout dégénéré (cellSize trop
		// grand pour ce mesh), retomber sur la géométrie d'origine plutôt que
		// de renvoyer un mesh vide.
		if (result.indices.empty())
			return source;

		return result;
	}
}
