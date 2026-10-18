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

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.16.tar.gz
        URL_HASH SHA256=c2ee715e42ec520c4d11fd8d249ef1d2b2baf4ad31148b72b3b000276c0b3633
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
FetchContent_MakeAvailable(SDL3 doctest starparse)

option(STARGBC_ENABLE_DEBUGGER "Build the native inspection and stepping frontend" ON)
if (STARGBC_ENABLE_DEBUGGER)
    FetchContent_Declare(imgui
            URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.tar.gz
            URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb
            EXCLUDE_FROM_ALL SYSTEM)
    FetchContent_MakeAvailable(imgui)
    add_library(stargbc_imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
    target_include_directories(stargbc_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(stargbc_imgui PUBLIC SDL3::SDL3-static)
endif ()

option(STARGBC_ENABLE_LUA "Build Lua scripting support" ON)
if (STARGBC_ENABLE_LUA)
    include(scripting/cmake/Lua.cmake)
endif ()

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

add_subdirectory(bootrom)
add_subdirectory(src)
add_subdirectory(frontends)
if (STARGBC_ENABLE_DEBUGGER)
    target_link_libraries(StarGBC PRIVATE StarGBC_DebuggerFrontend)
    target_compile_definitions(StarGBC PRIVATE STARGBC_ENABLE_DEBUGGER=1)
endif ()
if (STARGBC_ENABLE_LUA)
    add_subdirectory(scripting)
    target_link_libraries(StarGBC PRIVATE StarGBC_Scripting)
    target_compile_definitions(StarGBC PRIVATE STARGBC_ENABLE_LUA=1)
endif ()
add_subdirectory(diagnostics)
add_subdirectory(tests)
