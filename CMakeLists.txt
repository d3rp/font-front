cmake_minimum_required(VERSION 3.15)

project(font_front)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")

# https://docs.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=msvc-160
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /source-charset:utf-8)
endif()

# Freetype 2
find_package(Freetype REQUIRED)
add_library(freetype ALIAS Freetype::Freetype)

# Harfbuzz
add_subdirectory(deps/harfbuzz)

# GLFW 3
find_package(glfw3 REQUIRED)

# GLAD
add_library(glad deps/glad/src/glad.c)
target_compile_definitions(glad PUBLIC GLAD_GL_IMPLEMENTATION=1)
target_include_directories(glad PUBLIC "deps/glad/include")

# GLM
find_package(glm REQUIRED)

# utf8cpp
add_library(utf8cpp INTERFACE deps/utf8cpp/utf8string.hpp)
target_include_directories(utf8cpp INTERFACE "deps/utf8cpp")

target_link_libraries(${PROJECT_NAME}
        glfw
        glm::glm
        glad
        freetype
        harfbuzz
        utf8cpp
)