#pragma once

#include <string>
#include <filesystem>
#include <vector>

namespace Fufu 
{

	struct ProjectInfo
	{
		std::string name;
		std::string version = "0.1.0";
		std::filesystem::path rootDirectory;
		std::filesystem::path projectFilePath;
		std::string startupScene;
		std::vector<std::string> scenes;
		std::vector<std::string> assetDirectories;

		std::filesystem::path scenesDir() const { return rootDirectory / "Scenes"; }
		std::filesystem::path assetsDir() const { return rootDirectory / "Assets"; }
		std::filesystem::path configDir() const { return rootDirectory / "Config"; }
	};

}