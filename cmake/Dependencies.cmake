include(FetchContent)

if (STARGBC_BUILD_SDL_FRONTEND)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.16.tar.gz
            URL_HASH SHA256=c2ee715e42ec520c4d11fd8d249ef1d2b2baf4ad31148b72b3b000276c0b3633
            EXCLUDE_FROM_ALL SYSTEM)
    FetchContent_MakeAvailable(SDL3)

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

    if (STARGBC_ENABLE_LUA)
        include(${CMAKE_SOURCE_DIR}/scripting/cmake/Lua.cmake)
    endif ()
endif ()

if (STARGBC_BUILD_SDL_FRONTEND OR STARGBC_BUILD_DIAGNOSTICS)
    FetchContent_Declare(starparse
            GIT_REPOSITORY https://github.com/srikur/starparse.git
            GIT_TAG main
            GIT_SHALLOW ON
            EXCLUDE_FROM_ALL SYSTEM)
    FetchContent_MakeAvailable(starparse)
endif ()

if (STARGBC_BUILD_TESTS)
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(doctest
            URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
            URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
            EXCLUDE_FROM_ALL SYSTEM)
    FetchContent_MakeAvailable(doctest)
endif ()
