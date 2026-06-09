add_executable(FufuRuntime
    src/main.cpp
    src/RuntimeApp.cpp
)

target_include_directories(FufuRuntime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(FufuRuntime PRIVATE
    Fufu::Engine
)

# Strip les symboles en release (target_link_options n'existe qu'en 3.13+)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(FufuRuntime PROPERTIES
        LINK_FLAGS_RELEASE "-s"
    )
endif()