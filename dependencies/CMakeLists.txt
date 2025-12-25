set(SDL_TESTS OFF)
add_subdirectory(SDL)
add_subdirectory(doctest)
add_subdirectory(spdlog)

add_library(dependencies INTERFACE)
target_link_libraries(dependencies INTERFACE SDL3::SDL3)
target_link_libraries(dependencies INTERFACE spdlog::spdlog)