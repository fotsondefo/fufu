project(FufuRuntime
	VERSION 0.1.0
	LANGUAGES CXX
)

file(GLOB_RECURSE PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(FufuRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeMain.cpp
    ${PROJECT_SOURCES}
)

target_include_directories(FufuRuntime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(FufuRuntime PRIVATE
    Fufu::Engine
)

target_compile_options(FufuRuntime PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_SOURCE_DIR}/FufuEngine/src/depch.h>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_SOURCE_DIR}/FufuEngine/src/depch.h>
)

# Copier les assets � c�t� de l'ex�cutable
add_custom_command(TARGET FufuRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:FufuRuntime>/assets
    COMMENT "Cr�ation du dossier assets Runtime"
)

# Shaders moteur (compute path tracer, FXAA...) : voir FufuStudio/CMakeLists.txt
add_custom_command(TARGET FufuRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/FufuEngine/shaders
        $<TARGET_FILE_DIR:FufuRuntime>/shaders
    COMMENT "Copie des shaders moteur (Runtime)"
)