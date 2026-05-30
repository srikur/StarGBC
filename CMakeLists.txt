cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(your_target_name PRIVATE -Wall -Werror)
elseif(MSVC)
    target_compile_options(your_target_name PRIVATE /W4 /WX)
endif()

project(StarGBC VERSION 0.0.1 LANGUAGES CXX)

option(STARGBC_UPDATE_SUBMODULES "Init/update git submodules during configure" ON)
if(STARGBC_UPDATE_SUBMODULES AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    find_package(Git QUIET)
    if(GIT_FOUND)
        message(STATUS "Updating git submodules")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed (exit ${GIT_SUBMOD_RESULT}). Run it manually or pass -DSTARGBC_UPDATE_SUBMODULES=OFF to skip.")
        endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/dependencies/SDL/CMakeLists.txt")
    message(FATAL_ERROR "Submodules are missing. Run `git submodule update --init --recursive` or re-run CMake with -DSTARGBC_UPDATE_SUBMODULES=ON.")
endif()

add_subdirectory(dependencies EXCLUDE_FROM_ALL)
add_subdirectory(src)
add_subdirectory(tests)
