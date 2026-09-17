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

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.16.tar.gz
        URL_HASH SHA256=c2ee715e42ec520c4d11fd8d249ef1d2b2baf4ad31148b72b3b000276c0b3633
        EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
        URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
        EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
        URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
        EXCLUDE_FROM_ALL SYSTEM)
FetchContent_Declare(starparse
        GIT_REPOSITORY https://github.com/srikur/starparse.git
        GIT_TAG main
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL SYSTEM)
FetchContent_MakeAvailable(SDL3 spdlog doctest starparse)

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
