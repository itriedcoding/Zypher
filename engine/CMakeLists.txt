# Zypher engine library + runtime executable.
set(ZYPHER_CORE_SOURCES
    src/core/Assert.cpp
    src/core/Log.cpp
    src/core/Time.cpp
    src/core/Jobs.cpp
    src/core/Memory.cpp
    src/core/FileSystem.cpp
    src/core/Config.cpp
    src/core/Platform.cpp
    src/json/Json.cpp
    src/assets/PackFile.cpp
    src/assets/AssetRegistry.cpp
    src/assets/MeshBlob.cpp
    src/scene/Scene.cpp
    src/physics/PhysicsWorld.cpp
    src/nav/NavMesh.cpp
    src/render/RenderDevice.cpp
    src/render/SoftwareDevice.cpp
    src/render/MeshPrimitives.cpp
    src/render/PngWriter.cpp
    src/audio/AudioDevice.cpp
    src/scripting/ScriptHost.cpp
    src/profiling/Profiler.cpp
    src/crash/CrashReporter.cpp
    src/app/Application.cpp
)

if(WIN32)
    list(APPEND ZYPHER_CORE_SOURCES src/platform/PlatformWin32.cpp)
elseif(APPLE)
    list(APPEND ZYPHER_CORE_SOURCES src/platform/PlatformMac.mm)
else()
    list(APPEND ZYPHER_CORE_SOURCES src/platform/PlatformLinux.cpp)
endif()

if(ZYPHER_WITH_SDL3)
    list(APPEND ZYPHER_CORE_SOURCES src/platform/PlatformSDL3.cpp)
endif()

if(ZYPHER_WITH_VULKAN)
    list(APPEND ZYPHER_CORE_SOURCES src/render/vulkan/VulkanDevice.cpp)
endif()

if(ZYPHER_WITH_D3D12)
    list(APPEND ZYPHER_CORE_SOURCES src/render/d3d12/D3D12Device.cpp)
endif()

if(ZYPHER_WITH_METAL)
    list(APPEND ZYPHER_CORE_SOURCES src/render/metal/MetalDevice.mm)
endif()

if(ZYPHER_WITH_OPENGL)
    list(APPEND ZYPHER_CORE_SOURCES src/render/opengl/OpenGLDevice.cpp)
endif()

if(ZYPHER_WITH_MINIAUDIO)
    list(APPEND ZYPHER_CORE_SOURCES src/audio/MiniaudioDevice.cpp)
endif()

if(ZYPHER_WITH_LUA)
    list(APPEND ZYPHER_CORE_SOURCES src/scripting/LuaHost.cpp)
endif()

add_library(zypher_engine STATIC ${ZYPHER_CORE_SOURCES})
add_library(Zypher::Engine ALIAS zypher_engine)

target_include_directories(zypher_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
target_link_libraries(zypher_engine PUBLIC Threads::Threads)

target_compile_definitions(zypher_engine
    PUBLIC
        ZYPHER_VERSION_STRING="${PROJECT_VERSION}"
        ZYPHER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        ZYPHER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        ZYPHER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(WIN32)
    target_compile_definitions(zypher_engine PUBLIC ZYPHER_PLATFORM_WINDOWS=1)
    target_link_libraries(zypher_engine PUBLIC user32 gdi32 shell32 ole32)
elseif(APPLE)
    target_compile_definitions(zypher_engine PUBLIC ZYPHER_PLATFORM_MACOS=1)
    target_link_libraries(zypher_engine PUBLIC
        "-framework Cocoa" "-framework Foundation" "-framework IOKit" "-framework CoreVideo")
else()
    target_compile_definitions(zypher_engine PUBLIC ZYPHER_PLATFORM_LINUX=1)
endif()

if(APPLE)
    set_source_files_properties(src/platform/PlatformMac.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc")
endif()

# Feature switches are surfaced to the code as compile definitions.
foreach(feature
        ZYPHER_WITH_SDL3 ZYPHER_WITH_VULKAN ZYPHER_WITH_D3D12 ZYPHER_WITH_METAL
        ZYPHER_WITH_OPENGL ZYPHER_WITH_JOLT ZYPHER_WITH_BOX2D ZYPHER_WITH_RECAST
        ZYPHER_WITH_MINIAUDIO ZYPHER_WITH_ASSIMP ZYPHER_WITH_MESHOPT ZYPHER_WITH_IMGUI
        ZYPHER_WITH_FREETYPE ZYPHER_WITH_HARFBUZZ ZYPHER_WITH_TRACY ZYPHER_WITH_CRASHPAD
        ZYPHER_WITH_LUA ZYPHER_WITH_DOTNET)
    if(${feature})
        target_compile_definitions(zypher_engine PUBLIC ${feature}=1)
    endif()
endforeach()

if(ZYPHER_WITH_VULKAN)
    target_link_libraries(zypher_engine PRIVATE Vulkan::Vulkan)
endif()

if(ZYPHER_WITH_SDL3)
    target_link_libraries(zypher_engine PRIVATE SDL3::SDL3)
endif()

if(ZYPHER_WITH_MINIAUDIO)
    target_include_directories(zypher_engine PRIVATE ${MINIAUDIO_INCLUDE_DIR})
endif()

# ---------------------------------------------------------------------------
# Runtime executable
# ---------------------------------------------------------------------------
if(ZYPHER_BUILD_RUNTIME)
    add_executable(zypher_runtime src/main.cpp)
    target_link_libraries(zypher_runtime PRIVATE zypher_engine)
    set_target_properties(zypher_runtime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

install(TARGETS zypher_engine ARCHIVE DESTINATION lib)
if(ZYPHER_BUILD_RUNTIME)
    install(TARGETS zypher_runtime RUNTIME DESTINATION bin)
endif()
install(DIRECTORY include/zypher DESTINATION include)
install(FILES shaders/shader_manifest.json DESTINATION share/zypher/shaders)
