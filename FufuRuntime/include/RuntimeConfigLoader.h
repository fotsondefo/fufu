#pragma once

#include "RuntimeConfig.h"
#include <filesystem>

namespace FufuRuntime
{
	class RuntimeConfigLoader
	{
	public:
		static RuntimeConfig load(const std::filesystem::path& exeDir);
		static void save(const RuntimeConfig& config, const std::filesystem::path& exeDir);
	};
}