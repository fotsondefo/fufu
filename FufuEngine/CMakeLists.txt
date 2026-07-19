project(FufuEngine
		VERSION 0.1.0
		LANGUAGES CXX C
)

# List PROJECT_HEADERS
file(GLOB_RECURSE PROJECT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# List PROJECT_SOURCES
file(GLOB_RECURSE PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(FufuEngine STATIC ${PROJECT_SOURCES})

add_library(Fufu::Engine ALIAS FufuEngine)

target_include_directories(FufuEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# glad comme cible s�par�e en C pur � sans PCH C++
add_library(glad_internal STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/src/glad.c
)
target_include_directories(glad_internal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/include
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    FetchContent_Populate(glm)
    add_subdirectory(${glm_SOURCE_DIR} ${glm_BINARY_DIR})
endif()

if(TARGET glm)
    target_compile_options(glm INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/W0>
    )
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
    FetchContent_Populate(nlohmann_json)
    add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR})
endif()



FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()
# stb est header-only, on expose juste le r�pertoire
target_include_directories(FufuEngine PUBLIC ${stb_SOURCE_DIR})

target_link_libraries(FufuEngine
    PUBLIC
        glad_internal
        glfw
        spdlog::spdlog
        EnTT::EnTT
        nlohmann_json::nlohmann_json
        assimp::assimp
        glm
        $<$<PLATFORM_ID:Windows>:opengl32>
)

target_compile_options(FufuEngine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# PCH manuel compatible 3.12 (target_precompile_headers n'existe qu'en 3.16+)
# Chaque fichier .cpp inclut depch.h via /FI (MSVC) ou -include (GCC/Clang)
target_compile_options(FufuEngine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_CURRENT_SOURCE_DIR}/src/depch.h>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/depch.h>
)

# ── Backend Vulkan (optionnel, nécessite le Vulkan SDK) ──────────────────────
if(FUFU_BACKEND_VULKAN)
    # VulkanMemoryAllocator
    FetchContent_Declare(VMA
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.1.0
    )
    FetchContent_GetProperties(VMA)
    if(NOT VMA_POPULATED)
        FetchContent_Populate(VMA)
    endif()
    # VMA est header-only
    target_include_directories(FufuEngine PRIVATE ${vma_SOURCE_DIR}/include)

    # glslang pour la compilation GLSL → SPIR-V au runtime
    set(ENABLE_GLSLANG_BINARIES  OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER       OFF CACHE BOOL "" FORCE)
    set(ENABLE_GLSLANG_INSTALL   OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING            OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPT               OFF CACHE BOOL "" FORCE)  # pas de SPIRV-Tools
    FetchContent_Declare(glslang
        GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
        GIT_TAG        15.1.0
    )
    FetchContent_GetProperties(glslang)
    if(NOT glslang_POPULATED)
        FetchContent_Populate(glslang)
        add_subdirectory(${glslang_SOURCE_DIR} ${glslang_BINARY_DIR})
    endif()

    target_link_libraries(FufuEngine PRIVATE
        Vulkan::Vulkan
        glslang
        SPIRV
        glslang-default-resource-limits
    )
    target_compile_definitions(FufuEngine PUBLIC FUFU_BACKEND_VULKAN)
    target_include_directories(FufuEngine PRIVATE ${Vulkan_INCLUDE_DIRS})

    # Les TU VK reçoivent FUFU_VK_BACKEND pour neutraliser glad dans depch.h
    file(GLOB_RECURSE _VK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/RHI/VK/*.cpp")
    foreach(_f IN LISTS _VK_SOURCES)
        set_source_files_properties(${_f} PROPERTIES
            COMPILE_DEFINITIONS "FUFU_VK_BACKEND"
        )
    endforeach()
endif()