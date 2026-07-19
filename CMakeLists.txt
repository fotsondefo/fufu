cmake_minimum_required(VERSION 3.12...4.3)
project(Fufu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories centralisés
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Options de build
option(FUFU_BUILD_STUDIO    "Build FufuStudio (Editor)"  ON)
option(FUFU_BUILD_RUNTIME   "Build FufuRuntime"          ON)
option(FUFU_BUILD_LAB       "Build FufuLab (Research)"   ON)
option(FUFU_BACKEND_VULKAN  "Enable Vulkan backend"      ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0169 OLD)
endif()

# Dépendances externes via vcpkg ou find_package
find_package(OpenGL REQUIRED)

if(FUFU_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)
    message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
endif()

include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/TheCherno/glfw.git
    GIT_TAG        master
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(glfw)
if(NOT glfw_POPULATED)
    FetchContent_Populate(glfw)
    add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR})
endif()

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
    FetchContent_Populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

# EnTT
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.10.0
)
FetchContent_GetProperties(entt)
if(NOT entt_POPULATED)
    FetchContent_Populate(entt)
    add_subdirectory(${entt_SOURCE_DIR} ${entt_BINARY_DIR})
endif()

# assimp
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.2.5
)
set(ASSIMP_BUILD_TESTS        OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL            OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE) 
set(BUILD_SHARED_LIBS         OFF CACHE BOOL "" FORCE) 
FetchContent_GetProperties(assimp)
if(NOT assimp_POPULATED)
    FetchContent_Populate(assimp)
    add_subdirectory(${assimp_SOURCE_DIR} ${assimp_BINARY_DIR})
endif()

# nativefiledialog
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        v1.1.1
)
FetchContent_GetProperties(nfd)
if(NOT nfd_POPULATED)
    FetchContent_Populate(nfd)
    add_subdirectory(${nfd_SOURCE_DIR} ${nfd_BINARY_DIR})
endif()

# Sous-projets
add_subdirectory(FufuEngine)

if(FUFU_BUILD_STUDIO)
    add_subdirectory(FufuStudio)
endif()

if(FUFU_BUILD_RUNTIME)
    add_subdirectory(FufuRuntime)
endif()

if(FUFU_BUILD_LAB)
    add_subdirectory(FufuLab)
endif()