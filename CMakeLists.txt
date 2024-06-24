cmake_minimum_required(VERSION 3.15)

project(font_front)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(src)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")

# https://docs.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=msvc-160
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /source-charset:utf-8)
endif ()

# ICU
#include(FetchContent)
#FetchContent_Declare(
#        icu
#        GIT_REPOSITORY https://github.com/unicode-org/icu.git
#        GIT_TAG release-74-1  # Replace with the desired release tag
#)
#set(ICU_INSTALL_DIR ${CMAKE_SOURCE_DIR}/deps/icu)
#file(MAKE_DIRECTORY ${ICU_INSTALL_DIR})
#FetchContent_GetProperties(icu)
##if(NOT icu_POPULATED)
##    FetchContent_Populate(icu)
##
##    if (NOT icu_BINARY_DIR)
##        message(FATAL_ERROR "ICU fetch failed...")
##    endif()
##    # Set ICU build and install directories
##    set(ICU_BUILD_DIR ${icu_BINARY_DIR}/icu_build)
##    set(ICU_INSTALL_DIR ${ICU_INSTALL_DIR})
##    add_subdirectory(${icu_SOURCE_DIR}/icu4c/source ${ICU_BUILD_DIR})
##    set(CMAKE_INSTALL_PREFIX ${ICU_INSTALL_DIR})
##    add_custom_target(
##            icu-install
##            COMMAND ${CMAKE_COMMAND} --build ${ICU_BUILD_DIR} --target install
##    )
##endif()
# Check if on macOS and using Homebrew
if (APPLE)
    # Execute the brew command to get the ICU prefix
    execute_process(
            COMMAND brew --prefix icu4c
            OUTPUT_VARIABLE BREW_ICU_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Set CMAKE_PREFIX_PATH using the fetched value
    set(CMAKE_PREFIX_PATH ${BREW_ICU_PREFIX} ${CMAKE_PREFIX_PATH})
endif ()
message(STATUS "brew icu prefix: ${BREW_ICU_PREFIX}")
find_package(ICU REQUIRED COMPONENTS uc i18n data)
include_directories("${BREW_ICU_PREFIX}/include")

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
message(STATUS ${sheenbidi_sources})
add_library(sheenbidi ${sheenbidi_sources})
target_include_directories(sheenbidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/deps/sheenbidi/Headers)

target_link_libraries(${PROJECT_NAME}
        glfw
        glm::glm
        glad
        freetype
        harfbuzz
        utf8cpp
        ICU::uc ICU::i18n
        sheenbidi
)