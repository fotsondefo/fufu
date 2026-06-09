project(FufuEngine
		VERSION 0.1.0
		LANGUAGES CXX
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
# stb est header-only, on expose juste le répertoire
target_include_directories(FufuEngine PUBLIC ${stb_SOURCE_DIR})

target_link_libraries(FufuEngine
    PUBLIC
        OpenGL::GL
        glfw
        spdlog::spdlog
        EnTT::EnTT
        # yaml-cpp::yaml-cpp
        assimp::assimp
		glm
		nlohmann_json::nlohmann_json
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