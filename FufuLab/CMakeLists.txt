project(FufuLab
    VERSION 0.1.0
    LANGUAGES CXX
)

# Each demo is a subdirectory of `demos/` with its own `CMakeLists.txt`.
# This main target (FufuLab) provides the common shell: window, ImGui,
# and the LabLayer layer, which coordinates the loading of the active demo.

# ImGui
FetchContent_Declare(
    imgui_lab
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_GetProperties(imgui_lab)
if(NOT imgui_lab_POPULATED)
    FetchContent_Populate(imgui_lab)
endif()

set(IMGUI_LAB_DIR ${imgui_lab_SOURCE_DIR})
add_library(imgui_lab_lib STATIC
    ${IMGUI_LAB_DIR}/imgui.cpp
    ${IMGUI_LAB_DIR}/imgui_draw.cpp
    ${IMGUI_LAB_DIR}/imgui_tables.cpp
    ${IMGUI_LAB_DIR}/imgui_widgets.cpp
    ${IMGUI_LAB_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_LAB_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lab_lib PUBLIC
    ${IMGUI_LAB_DIR}
    ${IMGUI_LAB_DIR}/backends
)
target_link_libraries(imgui_lab_lib PUBLIC glfw Fufu::Engine)
target_compile_options(imgui_lab_lib PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
)

# Main Executable File
file(GLOB_RECURSE FUFULAB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(FufuLab
    ${CMAKE_CURRENT_SOURCE_DIR}/LabMain.cpp
    ${FUFULAB_SOURCES}
)

target_include_directories(FufuLab PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(FufuLab PRIVATE
    Fufu::Engine
    imgui_lab_lib
    nfd
)

target_compile_options(FufuLab PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0 /FI${CMAKE_SOURCE_DIR}/FufuEngine/src/depch.h>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_SOURCE_DIR}/FufuEngine/src/depch.h>
)

add_custom_command(TARGET FufuLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:FufuLab>/assets
    COMMENT "Creation of the asset folder (FufuLab)"
)

add_custom_command(TARGET FufuLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/FufuEngine/shaders
        $<TARGET_FILE_DIR:FufuLab>/shaders
    COMMENT "Copy of engine's shaders (FufuLab)"
)

# Demo subdirectories - anyone can add their own resources/shaders
add_subdirectory(demos)
