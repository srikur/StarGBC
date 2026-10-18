add_executable(${PROJECT_NAME} main.cpp SDLFrontend.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_Core starparse::starparse SDL3::SDL3-static)