cmake_minimum_required(VERSION 3.15)

project(font_front)

if (MSVC)
    set(CMAKE_CXX_STANDARD 20)
else ()
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE INTERNAL "" FORCE)
endif ()
#set(CMAKE_CXX_FLAGS_DEBUG "-O1")

add_subdirectory(src)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")
# https://docs.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=msvc-160
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif ()

# Verify that opengl is installed
find_package(OpenGL REQUIRED)

# Freetype 2
set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB TRUE CACHE BOOL " " FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_BZip2 TRUE CACHE BOOL " " FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_PNG TRUE CACHE BOOL " " FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz TRUE CACHE BOOL " " FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_BrotliDec TRUE CACHE BOOL " " FORCE)
add_subdirectory(deps/freetype)
#find_package(Freetype REQUIRED)
#add_library(freetype ALIAS Freetype::Freetype)

# utfcpp
target_compile_definitions(${PROJECT_NAME} PUBLIC UTF_CPP_CPLUSPLUS=201703L)
target_include_directories(${PROJECT_NAME} PUBLIC "deps/utfcpp")

# Harfbuzz
set(HB_BUILD_SUBSET OFF CACHE BOOL " " FORCE)
add_subdirectory(deps/harfbuzz)

# GLFW 3
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL " " FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL " " FORCE)
set(GLFW_INSTALL OFF CACHE BOOL " " FORCE)
add_subdirectory(deps/glfw)
#find_package(glfw3 REQUIRED)

# GLAD
add_library(glad deps/glad/src/glad.c)
target_link_libraries(glad PRIVATE ${OPENGL_LIBRARIES})
#target_compile_definitions(glad PRIVATE GLAD_GL_IMPLEMENTATION=1)
target_include_directories(glad PUBLIC "deps/glad/include" ${OPENGL_INCLUDE_DIRS})

# GLM
#find_package(glm REQUIRED)
add_subdirectory(deps/glm)
include_directories(deps/glm)

# sheenbidi
set(SBidi_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/sheenbidi/Source")
list(APPEND sheenbidi_sources
        "${SBidi_SOURCE_DIR}/BidiChain.c"
        "${SBidi_SOURCE_DIR}/BidiTypeLookup.c"
        "${SBidi_SOURCE_DIR}/BracketQueue.c"
        "${SBidi_SOURCE_DIR}/GeneralCategoryLookup.c"
        "${SBidi_SOURCE_DIR}/IsolatingRun.c"
        "${SBidi_SOURCE_DIR}/LevelRun.c"
        "${SBidi_SOURCE_DIR}/PairingLookup.c"
        "${SBidi_SOURCE_DIR}/RunQueue.c"
        "${SBidi_SOURCE_DIR}/SBAlgorithm.c"
        "${SBidi_SOURCE_DIR}/SBBase.c"
        "${SBidi_SOURCE_DIR}/SBCodepointSequence.c"
        "${SBidi_SOURCE_DIR}/SBLine.c"
        "${SBidi_SOURCE_DIR}/SBLog.c"
        "${SBidi_SOURCE_DIR}/SBMirrorLocator.c"
        "${SBidi_SOURCE_DIR}/SBParagraph.c"
        "${SBidi_SOURCE_DIR}/SBScriptLocator.c"
        "${SBidi_SOURCE_DIR}/ScriptLookup.c"
        "${SBidi_SOURCE_DIR}/ScriptStack.c"
        "${SBidi_SOURCE_DIR}/StatusStack.c"
)
add_library(sheenbidi ${sheenbidi_sources})
target_include_directories(sheenbidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/deps/sheenbidi/Headers)

target_link_libraries(${PROJECT_NAME}
        glfw
        glm::glm
        glad
        freetype
        harfbuzz
        sheenbidi
)