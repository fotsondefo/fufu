#include "depch.h"
#include "Renderer/ShaderUtils.h"

namespace Fufu
{

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
