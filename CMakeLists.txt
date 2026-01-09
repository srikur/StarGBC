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

add_subdirectory(dependencies EXCLUDE_FROM_ALL)
add_subdirectory(src)
add_subdirectory(tests)
