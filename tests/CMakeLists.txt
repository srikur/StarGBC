file(GLOB_RECURSE TEST_SOURCES *.cpp)

target_include_directories(${PROJECT_NAME}_Core PUBLIC "${CMAKE_SOURCE_DIR}/dependencies/doctest/doctest")
add_subdirectory(mocks)
add_executable(${PROJECT_NAME}_Tests ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME}_Tests PRIVATE ${PROJECT_NAME}_Core)