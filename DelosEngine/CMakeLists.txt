add_library(DelosEngine STATIC
    src/Application/Application.cpp
    src/Application/LayerStack.cpp
    src/ECS/Scene.cpp
    src/ECS/Entity.cpp
    src/Assets/AssetManager.cpp
    src/Assets/UUID.cpp
    src/Renderer/Renderer.cpp
)

# Alias propre pour les cibles consommatrices
add_library(Delos::Engine ALIAS DelosEngine)

target_include_directories(DelosEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(DelosEngine
    PUBLIC
        OpenGL::GL
        glfw
        spdlog::spdlog
        EnTT::EnTT
        yaml-cpp::yaml-cpp
        assimp::assimp
)

target_compile_options(DelosEngine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# PCH manuel compatible 3.12 (target_precompile_headers n'existe qu'en 3.16+)
# Chaque fichier .cpp inclut depch.h via /FI (MSVC) ou -include (GCC/Clang)
target_compile_options(DelosEngine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_CURRENT_SOURCE_DIR}/src/depch.h>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/depch.h>
)