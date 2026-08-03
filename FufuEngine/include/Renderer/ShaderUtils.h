#pragma once

#include <cstdint>
#include <string>
#include <filesystem>
#include <initializer_list>

namespace Fufu
{
	// Small compilation/link helpers shared by the passes (ComputePass,
	// FXAAPass...): avoids duplicating the same glGetShaderiv/glGetProgramiv
	// loop in each pass.
	uint32_t compileShader(uint32_t type, const std::string& source);
	uint32_t linkProgram(std::initializer_list<uint32_t> shaders);

	// Reads a .glsl/.vert/.frag/.comp file from the shaders/ directory (copied
	// next to the executable at build time, see CMakeLists.txt of
	// FufuStudio/FufuRuntime). `relativePath` is relative to that directory
	// (e.g. "PathTracer.comp"). Returns an empty string and logs an error if
	// the file is not found.
	std::string loadShaderSource(const std::filesystem::path& relativePath);
}
