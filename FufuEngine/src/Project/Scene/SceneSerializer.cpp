#include "depch.h"
#include "Project/Scene/SceneSerializer.h"
#include "Project/Scene/EntitySerialization.h"
#include "Project/Components.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace Fufu {

	// ----------------------------------------------------------------
	// Schema migration
	// ----------------------------------------------------------------
	// Called with the version read from the file before deserialize()
	// interprets the JSON: transforms `root` in-place until it
	// matches the schema of SceneSerializer::k_CurrentVersion.
	// Example for a future version 2:
	//   if (fromVersion < 2) { /* rename a field, add a default value... */ }
	static void migrateSceneJson(json& root, int fromVersion)
	{
		(void)root;
		(void)fromVersion;
		// Nothing to migrate: the only existing version is k_CurrentVersion.
	}

	// ----------------------------------------------------------------
	// Serialize
	// ----------------------------------------------------------------

	void SceneSerializer::serialize(const std::filesystem::path& path) const
	{
		json root;
		root["scene"] = m_Scene->getName();
		root["version"] = SceneSerializer::k_CurrentVersion;

		const RenderSettings& rs = m_Scene->getRenderSettings();
		root["renderSettings"] = {
			{ "mode",            static_cast<int>(rs.mode) },
			{ "technique",       static_cast<int>(rs.technique) },
			{ "aaMode",          static_cast<int>(rs.aaMode) },
			{ "maxBounces",      rs.maxBounces },
			{ "samplesPerPixel", rs.samplesPerPixel },
			{ "maxAccumFrames",  rs.maxAccumFrames },
			{ "exposure",        rs.exposure },
			{ "taaBlendFactor",  rs.taaBlendFactor },
			{ "resetOnMove",     rs.resetOnMove },
			{ "tonemapping",     static_cast<int>(rs.tonemapping) },
			{ "gamma",           rs.gamma },
			{ "shadowBias",      rs.shadowBias },
			{ "bloomEnabled",    rs.bloomEnabled },
			{ "bloomThreshold",  rs.bloomThreshold },
			{ "bloomKnee",       rs.bloomKnee },
			{ "bloomStrength",   rs.bloomStrength },
			{ "bloomIterations", rs.bloomIterations },
			{ "ssaoEnabled",     rs.ssaoEnabled },
			{ "ssaoSamples",     rs.ssaoSamples },
			{ "ssaoRadius",      rs.ssaoRadius },
			{ "ssaoBias",        rs.ssaoBias },
			{ "ssaoStrength",    rs.ssaoStrength },
			{ "dofEnabled",      rs.dofEnabled },
			{ "dofFocusDist",    rs.dofFocusDist },
			{ "dofFocusRange",   rs.dofFocusRange },
			{ "dofMaxBlur",      rs.dofMaxBlur },
			{ "dofSamples",      rs.dofSamples },
			{ "volEnabled",      rs.volEnabled },
			{ "volSteps",        rs.volSteps },
			{ "volDensity",      rs.volDensity },
			{ "volScattering",   rs.volScattering },
			{ "volAnisotropy",   rs.volAnisotropy },
			{ "volAmbient",      rs.volAmbient },
			{ "volMaxDist",      rs.volMaxDist }
		};

		const EnvironmentSettings& env = m_Scene->getEnvironment();
		root["environment"] = {
			{ "useSkybox",         env.useSkybox },
			{ "skyboxTexturePath", env.skyboxTexturePath },
			{ "skyboxIntensity",   env.skyboxIntensity }
		};

		root["entities"] = json::array();

		auto& reg = m_Scene->m_Registry;

		// Stable index (position in the file) independent of entt handles,
		// which may differ from one load to the next.
		std::unordered_map<entt::entity, int> indexMap;
		int nextIndex = 0;
		reg.each([&](entt::entity handle) { indexMap[handle] = nextIndex++; });

		reg.each([&](entt::entity handle) {
			root["entities"].push_back(serializeEntityToJson(handle, reg, indexMap));
		});

		std::ofstream file(path);
		FUFU_ASSERT(file.is_open(), "Failed to open file for writing: {}", path.string());
		file << root.dump(4); // 4-space indentation
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

		int fileVersion = root.value("version", 1);
		if (fileVersion > SceneSerializer::k_CurrentVersion)
		{
			FUFU_WARN("Scene '{}' was saved with a newer format (v{} > v{} supported) — loading best-effort",
				path.string(), fileVersion, SceneSerializer::k_CurrentVersion);
		}
		else if (fileVersion < SceneSerializer::k_CurrentVersion)
		{
			migrateSceneJson(root, fileVersion);
		}

		m_Scene->setName(root.at("scene").get<std::string>());

		// Scenes predating the addition of this block: keep the default RenderSettings
		// already in place on the Scene rather than failing.
		if (root.contains("renderSettings"))
		{
			const auto& rsJson = root.at("renderSettings");
			RenderSettings& rs = m_Scene->getRenderSettings();
			rs.mode            = static_cast<RenderMode>(rsJson.value("mode", static_cast<int>(rs.mode)));
			rs.technique       = static_cast<RenderTechnique>(rsJson.value("technique", static_cast<int>(rs.technique)));
			rs.aaMode          = static_cast<AAMode>(rsJson.value("aaMode", static_cast<int>(rs.aaMode)));
			rs.maxBounces      = rsJson.value("maxBounces", rs.maxBounces);
			rs.samplesPerPixel = rsJson.value("samplesPerPixel", rs.samplesPerPixel);
			rs.maxAccumFrames  = rsJson.value("maxAccumFrames", rs.maxAccumFrames);
			rs.exposure        = rsJson.value("exposure", rs.exposure);
			rs.taaBlendFactor  = rsJson.value("taaBlendFactor", rs.taaBlendFactor);
			rs.resetOnMove     = rsJson.value("resetOnMove", rs.resetOnMove);
			rs.tonemapping     = static_cast<ToneMappingOperator>(rsJson.value("tonemapping", static_cast<int>(rs.tonemapping)));
			rs.gamma           = rsJson.value("gamma", rs.gamma);
			rs.shadowBias      = rsJson.value("shadowBias", rs.shadowBias);
			rs.bloomEnabled    = rsJson.value("bloomEnabled",    rs.bloomEnabled);
			rs.bloomThreshold  = rsJson.value("bloomThreshold",  rs.bloomThreshold);
			rs.bloomKnee       = rsJson.value("bloomKnee",       rs.bloomKnee);
			rs.bloomStrength   = rsJson.value("bloomStrength",   rs.bloomStrength);
			rs.bloomIterations = rsJson.value("bloomIterations", rs.bloomIterations);
			rs.ssaoEnabled     = rsJson.value("ssaoEnabled",     rs.ssaoEnabled);
			rs.ssaoSamples     = rsJson.value("ssaoSamples",     rs.ssaoSamples);
			rs.ssaoRadius      = rsJson.value("ssaoRadius",      rs.ssaoRadius);
			rs.ssaoBias        = rsJson.value("ssaoBias",        rs.ssaoBias);
			rs.ssaoStrength    = rsJson.value("ssaoStrength",    rs.ssaoStrength);
			rs.dofEnabled      = rsJson.value("dofEnabled",      rs.dofEnabled);
			rs.dofFocusDist    = rsJson.value("dofFocusDist",    rs.dofFocusDist);
			rs.dofFocusRange   = rsJson.value("dofFocusRange",   rs.dofFocusRange);
			rs.dofMaxBlur      = rsJson.value("dofMaxBlur",      rs.dofMaxBlur);
			rs.dofSamples      = rsJson.value("dofSamples",      rs.dofSamples);
			rs.volEnabled      = rsJson.value("volEnabled",      rs.volEnabled);
			rs.volSteps        = rsJson.value("volSteps",        rs.volSteps);
			rs.volDensity      = rsJson.value("volDensity",      rs.volDensity);
			rs.volScattering   = rsJson.value("volScattering",   rs.volScattering);
			rs.volAnisotropy   = rsJson.value("volAnisotropy",   rs.volAnisotropy);
			rs.volAmbient      = rsJson.value("volAmbient",      rs.volAmbient);
			rs.volMaxDist      = rsJson.value("volMaxDist",      rs.volMaxDist);
		}

		if (root.contains("environment"))
		{
			const auto& envJson = root.at("environment");
			EnvironmentSettings& env = m_Scene->getEnvironment();
			env.useSkybox         = envJson.value("useSkybox", env.useSkybox);
			env.skyboxTexturePath = envJson.value("skyboxTexturePath", env.skyboxTexturePath);
			env.skyboxIntensity   = envJson.value("skyboxIntensity", env.skyboxIntensity);
		}

		// id (as written in the file) -> newly created entity.
		std::unordered_map<int, Entity> idToEntity;

		for (const auto& j : root.at("entities"))
		{
			Entity entity = createEntityFromJson(m_Scene, j);
			idToEntity[j.at("id").get<int>()] = entity;
		}

		// Second pass to rebuild the hierarchy (all entities must exist
		// before resolving parent/child links).
		for (const auto& j : root.at("entities"))
		{
			if (!j.contains("parent"))
				continue;

			auto childIt = idToEntity.find(j.at("id").get<int>());
			auto parentIt = idToEntity.find(j.at("parent").get<int>());

			if (childIt != idToEntity.end() && parentIt != idToEntity.end())
				m_Scene->setParent(childIt->second, parentIt->second);
		}

		FUFU_INFO("Scene '{}' deserialized from '{}'", m_Scene->getName(), path.string());
		return true;
	}

}
