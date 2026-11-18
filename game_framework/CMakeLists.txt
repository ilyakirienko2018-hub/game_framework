cmake_minimum_required(VERSION 3.24)
project(GameFramework VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(CheckLanguage)
include(FetchContent)

option(GF_ENABLE_NASM "Use NASM AABB kernels when a Windows x64 assembler is available" ON)
option(GF_ENABLE_GRAPHICS_TESTS "Run tests that require a desktop OpenGL context" ON)

set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(GLFW_LIBRARY_TYPE STATIC)
FetchContent_Declare(glfw
    URL "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/glfw-3.5.1.tar.gz"
    URL_HASH SHA256=5234f4f29473e9a06bc7847d8371858dd135d38466eeeaa652fdc9f8f9ff0c20
)
FetchContent_MakeAvailable(glfw)

add_library(glad STATIC ThirdParty/glad/src/gl.c)
target_include_directories(glad PUBLIC ThirdParty/glad/include)

add_library(Engine STATIC
    engine/Application.cpp
    engine/Application.hpp
    engine/Input.hpp
    engine/Logger.cpp
    engine/Logger.hpp
    engine/Math.hpp
    engine/Physics.cpp
    engine/Physics.hpp
    engine/Renderer.cpp
    engine/Renderer.hpp
    engine/World.hpp
)
target_compile_features(Engine PUBLIC cxx_std_20)
target_include_directories(Engine PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine>")
target_compile_definitions(Engine PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(Engine PRIVATE glfw glad)
if(WIN32)
    target_compile_definitions(Engine PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(Engine PRIVATE ole32 windowscodecs)
endif()

if(GF_ENABLE_NASM AND WIN32 AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(x64|X64|AMD64)$")
    check_language(ASM_NASM)
    if(CMAKE_ASM_NASM_COMPILER)
        enable_language(ASM_NASM)
        target_sources(Engine PRIVATE engine/AabbMasks.asm)
        target_compile_definitions(Engine PRIVATE $<$<COMPILE_LANGUAGE:CXX>:GF_USE_NASM>)
        set(GF_NASM_ENABLED TRUE)
    endif()
endif()
if(GF_NASM_ENABLED)
    message(STATUS "Game Framework AABB backend: NASM/SSE")
else()
    message(STATUS "Game Framework AABB backend: portable C++")
endif()

add_executable(Sandbox
    game_framework.cpp
    demo/Combat.cpp
    demo/Demo.cpp
    demo/Demo.hpp
    demo/Platform.cpp
)
target_link_libraries(Sandbox PRIVATE Engine)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Sandbox)

foreach(target IN ITEMS Engine Sandbox)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8>"
        )
    endif()
endforeach()

if(BUILD_TESTING)
    add_test(NAME Engine.Logic COMMAND Sandbox --self-test)
    if(GF_ENABLE_GRAPHICS_TESTS)
        add_test(NAME Sandbox.Loop COMMAND Sandbox --smoke-test)
        add_test(NAME Sandbox.OpenGL COMMAND Sandbox --render-test)
        set_tests_properties(Sandbox.Loop Sandbox.OpenGL PROPERTIES
            LABELS graphics
            RUN_SERIAL TRUE
            TIMEOUT 30
        )
    endif()
endif()
