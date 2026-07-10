#include "depch.h"
#include "Renderer/ShaderUtils.h"
#include <fstream>
#include <sstream>

namespace Fufu
{

	std::string loadShaderSource(const std::filesystem::path& relativePath)
	{
		// CWD == dossier de l'exécutable dans ce projet (même convention que
		// le dossier "config" de ProjectManager) : les .cpp/.vert/.frag/.comp
		// sont copiés à côté par le build (voir CMakeLists.txt).
		std::filesystem::path fullPath = std::filesystem::current_path() / "shaders" / relativePath;

		std::ifstream file(fullPath);
		if (!file.is_open())
		{
			FUFU_ERROR("loadShaderSource: fichier introuvable '{}'", fullPath.string());
			return {};
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	uint32_t compileShader(uint32_t type, const std::string& source)
	{
		uint32_t shader = glCreateShader(type);
		const char* src = source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		int success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			char log[1024];
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			FUFU_ERROR("Shader compile error:\n{}", log);
		}
		return shader;
	}

	uint32_t linkProgram(std::initializer_list<uint32_t> shaders)
	{
		uint32_t program = glCreateProgram();
		for (uint32_t s : shaders)
			glAttachShader(program, s);
		glLinkProgram(program);

		int success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			char log[1024];
			glGetProgramInfoLog(program, sizeof(log), nullptr, log);
			FUFU_ERROR("Program link error:\n{}", log);
		}
		return program;
	}

}
