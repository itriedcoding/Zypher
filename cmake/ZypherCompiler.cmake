# Shared compiler configuration for every Zypher target.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /EHsc /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(ZYPHER_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
    # /MP speeds up multi-project builds on MSVC.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/MP>)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter)
    if(ZYPHER_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
    if(APPLE)
        add_compile_options(-Wno-deprecated-declarations)
    endif()
endif()

if(ZYPHER_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Deployment target: macOS 13 covers Metal 3 and universal binaries.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "" FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
