#include "Helpers/MeshPicking.h"
#include <Project/Components.h>
#include <Application/Application.h>
#include <limits>

namespace FufuStudio
{
	MeshPickResult pickMesh(const Fufu::SubMesh& mesh, const glm::mat4& model,
		const glm::mat4& viewProj, glm::vec2 uv)
	{
		glm::vec2 ndcClick = { uv.x * 2.f - 1.f, 1.f - uv.y * 2.f };

		MeshPickResult result;
		float bestDepth = std::numeric_limits<float>::max();

		for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
		{
			glm::vec3 worldA = glm::vec3(model * glm::vec4(mesh.vertices[mesh.indices[i]].position, 1.f));
			glm::vec3 worldB = glm::vec3(model * glm::vec4(mesh.vertices[mesh.indices[i + 1]].position, 1.f));
			glm::vec3 worldC = glm::vec3(model * glm::vec4(mesh.vertices[mesh.indices[i + 2]].position, 1.f));

			glm::vec4 clipA = viewProj * glm::vec4(worldA, 1.f);
			glm::vec4 clipB = viewProj * glm::vec4(worldB, 1.f);
			glm::vec4 clipC = viewProj * glm::vec4(worldC, 1.f);

			if (clipA.w <= 0.f || clipB.w <= 0.f || clipC.w <= 0.f)
				continue;

			glm::vec2 ndcA = glm::vec2(clipA) / clipA.w;
			glm::vec2 ndcB = glm::vec2(clipB) / clipB.w;
			glm::vec2 ndcC = glm::vec2(clipC) / clipC.w;

			float denom = (ndcB.y - ndcC.y) * (ndcA.x - ndcC.x) + (ndcC.x - ndcB.x) * (ndcA.y - ndcC.y);
			if (glm::abs(denom) < 1e-8f) continue;

			float w0 = ((ndcB.y - ndcC.y) * (ndcClick.x - ndcC.x) + (ndcC.x - ndcB.x) * (ndcClick.y - ndcC.y)) / denom;
			float w1 = ((ndcC.y - ndcA.y) * (ndcClick.x - ndcC.x) + (ndcA.x - ndcC.x) * (ndcClick.y - ndcC.y)) / denom;
			float w2 = 1.f - w0 - w1;

			if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;

			float depth = w0 * clipA.w + w1 * clipB.w + w2 * clipC.w;
			if (depth < bestDepth)
			{
				bestDepth = depth;
				result.hit = true;
				result.faceIndex = i / 3;
				result.worldPosition = w0 * worldA + w1 * worldB + w2 * worldC;
			}
		}

		return result;
	}

	std::optional<ImVec2> worldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProj,
		ImVec2 imagePos, ImVec2 imageSize)
	{
		glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.f);
		if (clip.w <= 0.f) return std::nullopt;

		glm::vec2 ndc = glm::vec2(clip) / clip.w;
		return ImVec2(
			imagePos.x + (ndc.x * 0.5f + 0.5f) * imageSize.x,
			imagePos.y + (1.f - (ndc.y * 0.5f + 0.5f)) * imageSize.y
		);
	}

	std::shared_ptr<Fufu::MeshAsset> getMeshAssetForEntity(Fufu::Entity entity)
	{
		if (!entity.isValid() || !entity.hasComponent<Fufu::MeshComponent>())
			return nullptr;

		auto& mesh = entity.getComponent<Fufu::MeshComponent>();
		auto& pm = Fufu::Application::get().getProjectManager();
		if (!pm.hasProject()) return nullptr;

		return pm.getCurrentProject().getAssetManager().getMesh(mesh.meshPath);
	}
}
