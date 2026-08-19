file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM SRC_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDLFrontend.cpp")

add_library(${PROJECT_NAME}_Core STATIC ${SRC_FILES})
if(TARGET stargbc_bootroms)
    add_dependencies(${PROJECT_NAME}_Core stargbc_bootroms)
endif()
target_include_directories(${PROJECT_NAME}_Core PUBLIC "${CMAKE_SOURCE_DIR}/includes")
target_link_libraries(${PROJECT_NAME}_Core PRIVATE spdlog::spdlog)

add_executable(${PROJECT_NAME} main.cpp SDLFrontend.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_Core SDL3::SDL3-static)