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
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# List PROJECT_HEADERS
file(GLOB_RECURSE PROJECT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# List PROJECT_SOURCES
set(PROJECT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/EditorMain.cpp)
file(GLOB_RECURSE REC_SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(APPEND PROJECT_SOURCES ${REC_SOURCES_DIR})

# FufuStudio
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(FufuStudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(FufuStudio PRIVATE
    Fufu::Engine
    imgui
)

add_custom_command(TARGET FufuStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:FufuStudio>/assets
    COMMENT "Création du dossier assets"
)