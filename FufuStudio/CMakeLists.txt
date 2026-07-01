project(FufuStudio
    VERSION 0.1.0
    LANGUAGES CXX
)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# ImGui n'a pas de CMakeLists — on construit la cible manuellement
# imgui_internal.h est un header, pas un .cpp — ne pas l'ajouter aux sources
# Pas de IMGUI_USER_CONFIG ni IMGUI_ENABLE_DOCKING — le docking est natif en v1.90.4
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw Fufu::Engine)

FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        1.83
)

FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()

add_library(imguizmo STATIC
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
)
target_include_directories(imguizmo PUBLIC ${imguizmo_SOURCE_DIR})
target_link_libraries(imguizmo PUBLIC imgui)
target_compile_definitions(imguizmo PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# Sources FufuStudio
set(PROJECT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/EditorMain.cpp)
file(GLOB_RECURSE REC_SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(APPEND PROJECT_SOURCES ${REC_SOURCES_DIR})

add_executable(FufuStudio ${PROJECT_SOURCES})

target_include_directories(FufuStudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(FufuStudio PRIVATE
    Fufu::Engine
    imgui
	imguizmo
	nfd
)

# PCH forcé — expose FUFU_ASSERT, macros log, STL à tous les .cpp de FufuStudio
target_compile_options(FufuStudio PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_SOURCE_DIR}/FufuEngine/src/depch.h>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_SOURCE_DIR}/FufuEngine/src/depch.h>
)

add_custom_command(TARGET FufuStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:FufuStudio>/assets
    COMMENT "Création du dossier assets"
)