#include "depch.h"
#include "Project/Assets/MeshUtils.h"
#include <glm/geometric.hpp>
#include <glm/exponential.hpp>

namespace Fufu
{
	void MeshUtils::recomputeNormals(SubMesh& mesh)
	{
		std::vector<glm::vec3> accum(mesh.vertices.size(), glm::vec3(0.f));

		for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
		{
			uint32_t ia = mesh.indices[i];
			uint32_t ib = mesh.indices[i + 1];
			uint32_t ic = mesh.indices[i + 2];

			glm::vec3 faceNormal = glm::cross(
				mesh.vertices[ib].position - mesh.vertices[ia].position,
				mesh.vertices[ic].position - mesh.vertices[ia].position);

			accum[ia] += faceNormal;
			accum[ib] += faceNormal;
			accum[ic] += faceNormal;
		}

		for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
		{
			float lenSq = glm::dot(accum[i], accum[i]);
			if (lenSq > 1e-12f)
				mesh.vertices[i].normal = accum[i] * (1.f / glm::sqrt(lenSq));
		}
	}
}
