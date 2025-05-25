file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME}_Core STATIC ${SRC_FILES})
target_include_directories(${PROJECT_NAME}_Core PUBLIC "${CMAKE_SOURCE_DIR}/includes")
target_include_directories(${PROJECT_NAME}_Core PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/SDL/include")

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_Core)
target_link_libraries(${PROJECT_NAME}_Core PRIVATE SDL3::SDL3)