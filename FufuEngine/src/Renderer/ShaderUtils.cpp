#include "depch.h"
#include "Renderer/ShaderUtils.h"
#include <fstream>
#include <sstream>

namespace Fufu
{

	std::string loadShaderSource(const std::filesystem::path& relativePath)
	{
		// CWD == executable directory in this project (same convention as
		// the "config" folder of ProjectManager): .vert/.frag/.comp files
		// are copied alongside it by the build (see CMakeLists.txt).
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
		if (source.empty()) return 0;

		uint32_t shader = glCreateShader(type);
		const char* src = source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		int success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			char log[2048];
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			FUFU_ERROR("Shader compile error:\n{}", log);
			glDeleteShader(shader);
			return 0;
		}
		return shader;
	}

	uint32_t linkProgram(std::initializer_list<uint32_t> shaders)
	{
		// Abort if any stage failed to compile
		for (uint32_t s : shaders)
			if (!s) return 0;

		uint32_t program = glCreateProgram();
		for (uint32_t s : shaders)
			glAttachShader(program, s);
		glLinkProgram(program);

		int success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			char log[2048];
			glGetProgramInfoLog(program, sizeof(log), nullptr, log);
			FUFU_ERROR("Program link error:\n{}", log);
			glDeleteProgram(program);
			return 0;
		}
		return program;
	}


}
