#pragma once

#include <cstdint>
#include <string>
#include <initializer_list>

namespace Fufu
{
	// Petits helpers de compilation/link partagés par les passes (ComputePass,
	// FXAAPass...) : évite de dupliquer la même boucle glGetShaderiv/glGetProgramiv
	// dans chaque pass.
	uint32_t compileShader(uint32_t type, const std::string& source);
	uint32_t linkProgram(std::initializer_list<uint32_t> shaders);
}
