#include "depch.h"
#include "Project/Scene/EntitySerialization.h"
#include "Project/Components.h"

using json = nlohmann::json;

namespace Fufu
{
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

	json serializeEntityToJson(entt::entity handle, entt::registry& reg,
		const std::unordered_map<entt::entity, int>& indexMap)
	{
		json j;
		j["id"] = indexMap.at(handle);

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
		{
			entt::entity parentHandle = reg.get<ParentComponent>(handle).parent;

			// Le parent peut être hors du sous-arbre sérialisé (cas d'un prefab
			// dont la racine a un parent dans la scène) : dans ce cas on ne
			// l'écrit simplement pas, l'entité redevient racine à l'instanciation.
			auto it = indexMap.find(parentHandle);
			if (it != indexMap.end())
				j["parent"] = it->second;
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

		if (reg.all_of<PrefabInstanceComponent>(handle))
			j["prefabInstance"] = reg.get<PrefabInstanceComponent>(handle).prefabPath;

		if (reg.all_of<GroomComponent>(handle))
		{
			auto& g = reg.get<GroomComponent>(handle);
			j["groom"] = {
				{ "strandCount", g.strandCount },
				{ "segments",    g.segments    },
				{ "length",      g.length      },
				{ "thickness",   g.thickness   },
				{ "gravity",     g.gravity     },
				{ "randomness",  g.randomness  },
				{ "seed",        g.seed        },
				{ "color",       serializeVec4(g.color) }
			};
		}

		if (reg.all_of<LightComponent>(handle))
		{
			auto& l = reg.get<LightComponent>(handle);
			j["light"] = {
				{ "type",      l.type == LightType::Point ? "point" : "directional" },
				{ "color",     serializeVec3(l.color) },
				{ "intensity", l.intensity },
				{ "radius",    l.radius }
			};
		}

		return j;
	}

	Entity createEntityFromJson(Scene* scene, const json& j)
	{
		Entity entity = scene->createEntity(j.value("tag", "Entity"));

		// Transform déjà ajouté par createEntity, on écrase les valeurs
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

		if (j.contains("prefabInstance"))
			entity.addComponent<PrefabInstanceComponent>(j["prefabInstance"].get<std::string>());

		if (j.contains("groom"))
		{
			GroomComponent g;
			g.strandCount = j["groom"].value("strandCount", 200);
			g.segments = j["groom"].value("segments", 3);
			g.length = j["groom"].value("length", 0.3f);
			g.thickness = j["groom"].value("thickness", 0.01f);
			g.gravity = j["groom"].value("gravity", 0.3f);
			g.randomness = j["groom"].value("randomness", 0.3f);
			g.seed = j["groom"].value("seed", uint32_t(1));
			if (j["groom"].contains("color"))
				g.color = deserializeVec4(j["groom"]["color"]);
			entity.addComponent<GroomComponent>(g);
		}

		if (j.contains("light"))
		{
			LightComponent l;
			std::string type = j["light"].value("type", "directional");
			l.type = (type == "point") ? LightType::Point : LightType::Directional;
			if (j["light"].contains("color"))
				l.color = deserializeVec3(j["light"]["color"]);
			l.intensity = j["light"].value("intensity", 3.f);
			l.radius = j["light"].value("radius", 0.0087f);
			entity.addComponent<LightComponent>(l);
		}

		return entity;
	}
}
