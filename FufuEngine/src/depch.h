#pragma once

// STL
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <cassert>

// Logging
#include <spdlog/spdlog.h>

// Core
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "Events/Event.h"
#include "Events/ApplicationEvents.h"
#include "Events/KeyEvents.h"
#include "Events/MouseEvents.h"

// Engine macros
#define FUFU_TRACE(...)    ::spdlog::trace(__VA_ARGS__)
#define FUFU_INFO(...)     ::spdlog::info(__VA_ARGS__)
#define FUFU_WARN(...)     ::spdlog::warn(__VA_ARGS__)
#define FUFU_ERROR(...)    ::spdlog::error(__VA_ARGS__)
#define FUFU_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)

#define FUFU_ASSERT(x, ...) { if(!(x)) { FUFU_ERROR("Assertion failed: {}", __VA_ARGS__); assert(x); } }
