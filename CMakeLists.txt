cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

project(StarGBC VERSION 0.0.1 LANGUAGES CXX)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++26 STARGBC_CXX26_SUPPORTED)
if (NOT STARGBC_CXX26_SUPPORTED)
    message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER}) "
            "does not accept -std=c++26. StarGBC requires a C++26-capable compiler; "
            "select one with CXX=<compiler> or -DCMAKE_CXX_COMPILER=<compiler>.")
endif ()

# Ninja preferred but not required
if (NOT CMAKE_GENERATOR MATCHES "Ninja")
    find_program(STARGBC_NINJA_EXECUTABLE ninja)
    if (STARGBC_NINJA_EXECUTABLE)
        message(STATUS "Ninja is installed but the ${CMAKE_GENERATOR} generator is in use; "
                "configure with CMAKE_GENERATOR=Ninja (or -G Ninja) in a fresh build directory for faster builds")
    endif ()
endif ()

include(CheckIPOSupported)
check_ipo_supported(RESULT STARGBC_IPO_SUPPORTED OUTPUT STARGBC_IPO_ERROR LANGUAGES CXX)
if (NOT STARGBC_IPO_SUPPORTED)
    message(STATUS "IPO/LTO not supported: ${STARGBC_IPO_ERROR}")
endif ()

option(STARGBC_BUILD_SDL_FRONTEND "Build the SDL desktop frontend" ON)
option(STARGBC_ENABLE_DEBUGGER "Build the imgui inspection and stepping window into the SDL frontend" ON)
option(STARGBC_ENABLE_LUA "Build Lua scripting support into the SDL frontend" ON)
option(STARGBC_BUILD_DIAGNOSTICS "Build the diagnostics library and command-line tools" ON)
option(STARGBC_BUILD_TESTS "Build the doctest test binary" ON)
if (STARGBC_BUILD_TESTS AND NOT STARGBC_BUILD_DIAGNOSTICS)
    message(STATUS "STARGBC_BUILD_TESTS requires the diagnostics library; enabling STARGBC_BUILD_DIAGNOSTICS")
    set(STARGBC_BUILD_DIAGNOSTICS ON CACHE BOOL "Build the diagnostics library and command-line tools" FORCE)
endif ()

include(cmake/Dependencies.cmake)

if (STARGBC_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON CACHE BOOL
            "Enable link-time optimization in Release builds")
endif ()
check_cxx_compiler_flag(-freflection STARGBC_FREFLECTION_SUPPORTED)
if (STARGBC_FREFLECTION_SUPPORTED)
    add_compile_options(-freflection)
endif ()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_link_options(-static-libstdc++ -static-libgcc)
    if (APPLE)
        # The macOS SDK's Annex K declarations (rsize_t/errno_t) are
        # clang-only; SDL_stdinc.h requests them unless this is pre-defined to 0
        add_compile_definitions(__STDC_WANT_LIB_EXT1__=0)
    endif ()
endif ()
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Werror)
elseif (MSVC)
    add_compile_options(/W4 /WX)
endif ()

add_subdirectory(core)
add_subdirectory(frontends)
if (STARGBC_BUILD_DIAGNOSTICS)
    add_subdirectory(diagnostics)
endif ()
if (STARGBC_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
