#include "depch.h"
#include "Project/Scene/SceneSerializer.h"
#include "Project/Components.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace Fufu {

	// ----------------------------------------------------------------
	// Helpers de sérialisation des types glm
	// ----------------------------------------------------------------

	static json serializeVec3(const glm::vec3& v)
	{
		return { {"x", v.x}, {"y", v.y}, {"z", v.z} };
	}

	static json serializeVec4(const glm::vec4& v)
	{
		return { {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} };
	}

	static glm::vec3 deserializeVec3(const json& j)
	{
		return { j.at("x").get<float>(), j.at("y").get<float>(), j.at("z").get<float>() };
	}

	static glm::vec4 deserializeVec4(const json& j)
	{
		return { j.at("x").get<float>(), j.at("y").get<float>(),
				 j.at("z").get<float>(), j.at("w").get<float>() };
	}

	// ----------------------------------------------------------------
	// Sérialisation d'une entité
	// ----------------------------------------------------------------

	static json serializeEntity(const Scene& scene, entt::entity handle, entt::registry& reg)
	{
		json j;
		j["id"] = static_cast<uint32_t>(handle);

		if (reg.all_of<TagComponent>(handle))
			j["tag"] = reg.get<TagComponent>(handle).tag;

		if (reg.all_of<TransformComponent>(handle))
		{
			auto& t = reg.get<TransformComponent>(handle);
			j["transform"] = {
				{ "position", serializeVec3(t.position) },
				{ "rotation", serializeVec3(t.rotation) },
				{ "scale",    serializeVec3(t.scale)    }
			};
		}

		if (reg.all_of<ParentComponent>(handle))
			j["parent"] = static_cast<uint32_t>(reg.get<ParentComponent>(handle).parent);

		if (reg.all_of<ChildrenComponent>(handle))
		{
			json children = json::array();
			for (entt::entity child : reg.get<ChildrenComponent>(handle).children)
				children.push_back(static_cast<uint32_t>(child));
			j["children"] = children;
		}

		if (reg.all_of<MeshComponent>(handle))
		{
			auto& m = reg.get<MeshComponent>(handle);
			j["mesh"] = {
				{ "path",   m.meshPath },
				{ "meshID", m.meshID   }
			};
		}

		if (reg.all_of<MaterialComponent>(handle))
		{
			auto& mat = reg.get<MaterialComponent>(handle);
			j["material"] = {
				{ "albedo",      serializeVec4(mat.albedo) },
				{ "metallic",    mat.metallic              },
				{ "roughness",   mat.roughness             },
				{ "emissive",    mat.emissive              },
				{ "albedoTexID", mat.albedoTexID           },
				{ "normalTexID", mat.normalTexID           }
			};
		}

		if (reg.all_of<CameraComponent>(handle))
		{
			auto& cam = reg.get<CameraComponent>(handle);
			j["camera"] = {
				{ "projection", cam.projection == CameraProjection::Perspective ? "perspective" : "orthographic" },
				{ "fov",        cam.fov        },
				{ "nearPlane",  cam.nearPlane  },
				{ "farPlane",   cam.farPlane   },
				{ "orthoSize",  cam.orthoSize  },
				{ "primary",    cam.primary    }
			};
		}

		return j;
	}

	// ----------------------------------------------------------------
	// Serialize
	// ----------------------------------------------------------------

	void SceneSerializer::serialize(const std::filesystem::path& path) const
	{
		json root;
		root["scene"] = m_Scene->getName();
		root["version"] = 1;
		root["entities"] = json::array();

		auto& reg = m_Scene->m_Registry;
		reg.each([&](entt::entity handle) {
			root["entities"].push_back(serializeEntity(*m_Scene, handle, reg));
		});

		std::ofstream file(path);
		FUFU_ASSERT(file.is_open(), "Failed to open file for writing: {}", path.string());
		file << root.dump(4); // indentation 4 espaces
		FUFU_INFO("Scene '{}' serialized to '{}'", m_Scene->getName(), path.string());
	}

	// ----------------------------------------------------------------
	// Deserialize
	// ----------------------------------------------------------------

	bool SceneSerializer::deserialize(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			FUFU_ERROR("Failed to open scene file: {}", path.string());
			return false;
		}

		json root;
		try { root = json::parse(file); }
		catch (const json::parse_error& e)
		{
			FUFU_ERROR("JSON parse error in '{}': {}", path.string(), e.what());
			return false;
		}

		m_Scene->setName(root.at("scene").get<std::string>());

		for (const auto& j : root.at("entities"))
		{
			Entity entity = m_Scene->createEntity(
				j.value("tag", "Entity")
			);

			// Transform — déjà ajouté par createEntity, on écrase les valeurs
			if (j.contains("transform"))
			{
				auto& t = entity.getComponent<TransformComponent>();
				t.position = deserializeVec3(j["transform"]["position"]);
				t.rotation = deserializeVec3(j["transform"]["rotation"]);
				t.scale = deserializeVec3(j["transform"]["scale"]);
			}

			if (j.contains("mesh"))
			{
				entity.addComponent<MeshComponent>(
					j["mesh"].at("path").get<std::string>(),
					j["mesh"].at("meshID").get<uint64_t>()
					);
			}

			if (j.contains("material"))
			{
				MaterialComponent mat;
				mat.albedo = deserializeVec4(j["material"]["albedo"]);
				mat.metallic = j["material"].value("metallic", 0.f);
				mat.roughness = j["material"].value("roughness", 1.f);
				mat.emissive = j["material"].value("emissive", 0.f);
				mat.albedoTexID = j["material"].value("albedoTexID", uint64_t(0));
				mat.normalTexID = j["material"].value("normalTexID", uint64_t(0));
				entity.addComponent<MaterialComponent>(mat);
			}

			if (j.contains("camera"))
			{
				CameraComponent cam;
				std::string proj = j["camera"].value("projection", "perspective");
				cam.projection = (proj == "orthographic")
					? CameraProjection::Orthographic
					: CameraProjection::Perspective;
				cam.fov = j["camera"].value("fov", glm::radians(45.f));
				cam.nearPlane = j["camera"].value("nearPlane", 0.1f);
				cam.farPlane = j["camera"].value("farPlane", 1000.f);
				cam.orthoSize = j["camera"].value("orthoSize", 10.f);
				cam.primary = j["camera"].value("primary", false);
				entity.addComponent<CameraComponent>(cam);
			}
		}

		// Deuxième passe pour reconstruire la hiérarchie
		// (les entités enfants doivent exister avant de linker)
		for (const auto& j : root.at("entities"))
		{
			if (!j.contains("parent"))
				continue;

			entt::entity childHandle = static_cast<entt::entity>(j.at("id").get<uint32_t>());
			entt::entity parentHandle = static_cast<entt::entity>(j.at("parent").get<uint32_t>());

			if (m_Scene->m_Registry.valid(childHandle) && m_Scene->m_Registry.valid(parentHandle))
			{
				Entity child(childHandle, m_Scene);
				Entity parent(parentHandle, m_Scene);
				m_Scene->setParent(child, parent);
			}
		}

		FUFU_INFO("Scene '{}' deserialized from '{}'", m_Scene->getName(), path.string());
		return true;
	}

}