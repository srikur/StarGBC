file(GLOB_RECURSE TEST_SOURCES *.cpp)

add_subdirectory(mocks)
add_executable(${PROJECT_NAME}_Tests ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME}_Tests PRIVATE ${PROJECT_NAME}_Core doctest::doctest)