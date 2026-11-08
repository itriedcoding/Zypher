# Feature switches for Zypher.
#
# The engine core, the asset pipeline, the editor core and the headless runtime
# are dependency free so that a fresh checkout builds with nothing but a C++23
# compiler. Everything below is an opt-in integration with an external SDK or
# library; integrations that are not wired up yet fail fast instead of producing
# a confusing missing-source error. See docs/roadmap.md for the status table.

option(ZYPHER_BUILD_RUNTIME "Build the zypher_runtime executable" ON)
option(ZYPHER_BUILD_TESTS   "Build the unit and integration tests" ON)
option(ZYPHER_BUILD_SAMPLES "Build the sample projects" OFF)
option(ZYPHER_BUILD_EDITOR  "Build the editor core (and the Qt 6 shell when Qt is found)" OFF)

option(ZYPHER_WITH_SDL3     "Use SDL3 for windowing and input" OFF)
option(ZYPHER_WITH_VULKAN   "Build the Vulkan rendering backend" OFF)
option(ZYPHER_WITH_D3D12    "Build the Direct3D 12 rendering backend (Windows)" OFF)
option(ZYPHER_WITH_METAL    "Build the Metal rendering backend (macOS)" OFF)
option(ZYPHER_WITH_OPENGL   "Build the legacy OpenGL backend" OFF)
option(ZYPHER_WITH_SLANG    "Compile shaders from Slang sources at build time" OFF)
option(ZYPHER_WITH_JOLT     "Use Jolt Physics for 3D rigid bodies" OFF)
option(ZYPHER_WITH_BOX2D    "Use Box2D for 2D rigid bodies" OFF)
option(ZYPHER_WITH_RECAST   "Use Recast/Detour for navigation meshes" OFF)
option(ZYPHER_WITH_MINIAUDIO "Use miniaudio for audio output" OFF)
option(ZYPHER_WITH_ASSIMP   "Use Assimp for additional model formats at cook time" OFF)
option(ZYPHER_WITH_MESHOPT  "Use meshoptimizer for vertex/index optimisation" OFF)
option(ZYPHER_WITH_IMGUI    "Build the Dear ImGui debug overlay" OFF)
option(ZYPHER_WITH_FREETYPE "Use FreeType for font rasterisation" OFF)
option(ZYPHER_WITH_HARFBUZZ "Use HarfBuzz for text shaping" OFF)
option(ZYPHER_WITH_TRACY    "Instrument with the Tracy profiler" OFF)
option(ZYPHER_WITH_CRASHPAD "Enable Crashpad crash reporting" OFF)
option(ZYPHER_WITH_LUA      "Embed the Lua scripting runtime" OFF)
option(ZYPHER_WITH_DOTNET   "Load C# gameplay modules through the native script ABI" ON)

option(ZYPHER_ENABLE_SANITIZERS "Build with address+undefined sanitizers" OFF)
option(ZYPHER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(ZYPHER_WITH_D3D12 AND NOT WIN32)
    message(FATAL_ERROR "ZYPHER_WITH_D3D12 is only available on Windows")
endif()

if(ZYPHER_WITH_METAL AND NOT APPLE)
    message(FATAL_ERROR "ZYPHER_WITH_METAL is only available on Apple platforms")
endif()

set(ZYPHER_EDITOR_QT_MIN_VERSION "6.5" CACHE STRING "Minimum Qt 6 version for the editor")

# The editor core (session, undo/redo, project model) has no Qt dependency and
# is always compiled with ZYPHER_BUILD_EDITOR=ON. The QML shell is optional.
set(ZYPHER_EDITOR_QT OFF)
if(ZYPHER_BUILD_EDITOR)
    find_package(Qt6 ${ZYPHER_EDITOR_QT_MIN_VERSION} QUIET COMPONENTS Core Gui Qml Quick QuickControls2)
    if(Qt6_FOUND)
        set(ZYPHER_EDITOR_QT ON)
        message(STATUS "Qt ${Qt6_VERSION} found: building the QML editor shell")
    else()
        message(STATUS "Qt ${ZYPHER_EDITOR_QT_MIN_VERSION}+ not found: building the editor core only "
                       "(install Qt 6.5+ and set Qt6_DIR to build the desktop shell)")
    endif()
endif()

# Integrations that are planned but not wired up in this milestone. Failing here
# keeps the build honest: enabling one never silently does nothing.
foreach(unimplemented
        ZYPHER_WITH_SDL3 ZYPHER_WITH_VULKAN ZYPHER_WITH_D3D12 ZYPHER_WITH_METAL
        ZYPHER_WITH_OPENGL ZYPHER_WITH_JOLT ZYPHER_WITH_BOX2D ZYPHER_WITH_RECAST
        ZYPHER_WITH_MINIAUDIO ZYPHER_WITH_ASSIMP ZYPHER_WITH_MESHOPT ZYPHER_WITH_IMGUI
        ZYPHER_WITH_FREETYPE ZYPHER_WITH_HARFBUZZ ZYPHER_WITH_TRACY ZYPHER_WITH_CRASHPAD
        ZYPHER_WITH_LUA ZYPHER_WITH_SLANG)
    if(${unimplemented})
        message(FATAL_ERROR
            "${unimplemented}=ON is not implemented in Zypher 0.1.0. The dependency free "
            "platform layer, software rasteriser, null audio device and native C# script host "
            "are used instead. See docs/roadmap.md for the integration plan.")
    endif()
endforeach()
