#include "RuntimeConfigLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace FufuRuntime
{
	static const char* k_ConfigFile = "fufu.runtimeconfig";

	RuntimeConfig RuntimeConfigLoader::load(const std::filesystem::path& exeDir)
	{
		RuntimeConfig config;
		std::filesystem::path configPath = exeDir / k_ConfigFile;

		std::ifstream file(configPath);
		if (!file.is_open())
		{
			FUFU_WARN("RuntimeConfig not found at '{}', using defaults", configPath.string());

			save(config, exeDir);
			return config;
		}

		json jsonFile;

		try
		{
			jsonFile = json::parse(file);
		}
		catch (const json::parse_error& e)
		{
			FUFU_ERROR("RuntimeConfig parse error: {}", e.what());
			return config;
		}

		config.scenePath = jsonFile.value("scenePath", config.scenePath);
		config.windowTitle = jsonFile.value("windowTitle", config.windowTitle);
		config.width = jsonFile.value("width", config.width);
		config.height = jsonFile.value("height", config.height);
		config.vsync = jsonFile.value("vsync", config.vsync);
		config.maxBounces = jsonFile.value("maxBounces", config.maxBounces);
		config.samplesPerPixel = jsonFile.value("samplesPerPixel", config.samplesPerPixel);
		config.exposure = jsonFile.value("exposure", config.exposure);

		FUFU_INFO("RuntimeConfig loaded from '{}'", configPath.string());
		return config;
	}

	void RuntimeConfigLoader::save(const RuntimeConfig& config, const std::filesystem::path& exeDir)
	{
		json jsonFile;

		jsonFile["scenePath"] = config.scenePath;
		jsonFile["windowTitle"] = config.windowTitle;
		jsonFile["width"] = config.width;
		jsonFile["height"] = config.height;
		jsonFile["vsync"] = config.vsync;
		jsonFile["maxBounces"] = config.maxBounces;
		jsonFile["samplesPerPixel"] = config.samplesPerPixel;
		jsonFile["exposure"] = config.exposure;
		
		std::ofstream file(exeDir / k_ConfigFile);
		if (file.is_open())
		{
			file << jsonFile.dump(4);
		}
	}
}