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

target_link_libraries(FufuEngine
    PUBLIC
        OpenGL::GL
        glfw
        spdlog::spdlog
        EnTT::EnTT
        yaml-cpp::yaml-cpp
        assimp::assimp
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