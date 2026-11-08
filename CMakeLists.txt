# Zypher engine - top level build entry point.
#
# Zypher targets macOS and Windows. A Linux build is supported for headless
# continuous-integration verification of the engine core (it uses the same
# platform-independent code paths plus the null/software devices).
cmake_minimum_required(VERSION 3.24)

project(Zypher
    VERSION 0.1.2
    DESCRIPTION "Zypher - an open source game engine for macOS and Windows"
    HOMEPAGE_URL "https://github.com/itriedcoding/Zypher"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ZypherCompiler)   # warnings, sanitizers, arch flags
include(ZypherOptions)    # feature switches and third party discovery

if(APPLE)
    enable_language(OBJCXX)
endif()

add_subdirectory(engine)

if(ZYPHER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ZYPHER_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(ZYPHER_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Zypher ${PROJECT_VERSION} (${CMAKE_BUILD_TYPE})")
message(STATUS "  system          : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  compiler        : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  cxx standard    : 23")
message(STATUS "  runtime         : ${ZYPHER_BUILD_RUNTIME}")
message(STATUS "  tests           : ${ZYPHER_BUILD_TESTS}")
message(STATUS "  editor          : ${ZYPHER_BUILD_EDITOR}")
message(STATUS "  gpu backends    : vulkan=${ZYPHER_WITH_VULKAN} d3d12=${ZYPHER_WITH_D3D12} metal=${ZYPHER_WITH_METAL}")
message(STATUS "  optional libs   : sdl3=${ZYPHER_WITH_SDL3} jolt=${ZYPHER_WITH_JOLT} box2d=${ZYPHER_WITH_BOX2D} lua=${ZYPHER_WITH_LUA} dotnet=${ZYPHER_WITH_DOTNET}")
message(STATUS "")
