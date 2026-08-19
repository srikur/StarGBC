cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

project(StarGBC VERSION 0.0.1 LANGUAGES CXX)

include(CheckIPOSupported)
check_ipo_supported(RESULT STARGBC_IPO_SUPPORTED OUTPUT STARGBC_IPO_ERROR LANGUAGES CXX)
if(NOT STARGBC_IPO_SUPPORTED)
    message(STATUS "IPO/LTO not supported: ${STARGBC_IPO_ERROR}")
endif()

# Dependencies are prebuilt static libraries installed by scripts/build-deps.sh; the local prefix is prepended so it wins over any system/Homebrew installs.
list(PREPEND CMAKE_PREFIX_PATH "${PROJECT_SOURCE_DIR}/dependencies/install")
find_package(SDL3 CONFIG QUIET)
find_package(spdlog CONFIG QUIET)
find_package(doctest CONFIG QUIET)
if(NOT SDL3_FOUND OR NOT spdlog_FOUND OR NOT doctest_FOUND)
    message(FATAL_ERROR "Missing dependencies (SDL3 found: ${SDL3_FOUND}, spdlog found: ${spdlog_FOUND}, doctest found: ${doctest_FOUND}). Run ./scripts/build-deps.sh once to build them into dependencies/install/, or point CMAKE_PREFIX_PATH at an install that provides them.")
endif()
if(NOT TARGET SDL3::SDL3-static)
    message(FATAL_ERROR "SDL3 was found at ${SDL3_DIR} but provides no static library. Run ./scripts/build-deps.sh to build the static SDL3 into dependencies/install/.")
endif()

if(STARGBC_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # C++26 reflection (experimental in GCC 16); static runtimes so the binary
    # doesn't depend on Homebrew's libstdc++/libgcc dylibs
    add_compile_options(-freflection)
    add_link_options(-static-libstdc++ -static-libgcc)
    if(APPLE)
        # The macOS SDK's Annex K declarations (rsize_t/errno_t) are
        # clang-only; SDL_stdinc.h requests them unless this is pre-defined to 0
        add_compile_definitions(__STDC_WANT_LIB_EXT1__=0)
    endif()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Werror)
elseif(MSVC)
    add_compile_options(/W4 /WX)
endif()

add_subdirectory(bootrom)
add_subdirectory(src)
add_subdirectory(tests)
