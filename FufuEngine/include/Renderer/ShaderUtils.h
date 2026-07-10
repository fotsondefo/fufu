#pragma once

#include <cstdint>
#include <string>
#include <filesystem>
#include <initializer_list>

namespace Fufu
{
	// Petits helpers de compilation/link partagés par les passes (ComputePass,
	// FXAAPass...) : évite de dupliquer la même boucle glGetShaderiv/glGetProgramiv
	// dans chaque pass.
	uint32_t compileShader(uint32_t type, const std::string& source);
	uint32_t linkProgram(std::initializer_list<uint32_t> shaders);

	// Lit un fichier .glsl/.vert/.frag/.comp sous shaders/ (copié à côté de
	// l'exécutable au build, voir CMakeLists.txt de FufuStudio/FufuRuntime).
	// `relativePath` est relatif à ce dossier (ex: "PathTracer.comp").
	// Renvoie une chaîne vide et logge une erreur si le fichier est introuvable.
	std::string loadShaderSource(const std::filesystem::path& relativePath);
}
