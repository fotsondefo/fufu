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

// Macros de log propres au moteur
#define DELOS_TRACE(...)    ::spdlog::trace(__VA_ARGS__)
#define DELOS_INFO(...)     ::spdlog::info(__VA_ARGS__)
#define DELOS_WARN(...)     ::spdlog::warn(__VA_ARGS__)
#define DELOS_ERROR(...)    ::spdlog::error(__VA_ARGS__)
#define DELOS_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)

#define DELOS_ASSERT(x, ...) { if(!(x)) { DELOS_ERROR("Assertion failed: {}", __VA_ARGS__); assert(x); } }
