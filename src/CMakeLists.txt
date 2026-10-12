file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM SRC_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDLFrontend.cpp")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/includes/*.h" "${CMAKE_SOURCE_DIR}/diagnostics/include/*.h")
set(CORE_BUILD_IDENTITY "${CMAKE_CURRENT_BINARY_DIR}/generated/CoreBuildIdentity.cpp")
add_custom_command(
        OUTPUT "${CORE_BUILD_IDENTITY}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/build-identity.py"
                "${CORE_BUILD_IDENTITY}" "${CMAKE_SOURCE_DIR}"
                "${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_FLAGS}-${CMAKE_CXX_FLAGS_RELEASE}-${CMAKE_CXX_FLAGS_DEBUG}"
                ${SRC_FILES} ${CORE_HEADERS} "${CMAKE_SOURCE_DIR}/CMakeLists.txt" "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
        DEPENDS ${SRC_FILES} ${CORE_HEADERS} "${CMAKE_SOURCE_DIR}/scripts/build-identity.py" "${CMAKE_SOURCE_DIR}/CMakeLists.txt" "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
        VERBATIM)
add_library(${PROJECT_NAME}_Core STATIC ${SRC_FILES} "${CORE_BUILD_IDENTITY}")
if(TARGET stargbc_bootroms)
    add_dependencies(${PROJECT_NAME}_Core stargbc_bootroms)
endif()
target_include_directories(${PROJECT_NAME}_Core PUBLIC "${CMAKE_SOURCE_DIR}/includes")
target_include_directories(${PROJECT_NAME}_Core PUBLIC "${CMAKE_SOURCE_DIR}/diagnostics/include")
target_link_libraries(${PROJECT_NAME}_Core PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME}_Core PUBLIC starparse::starparse)

add_executable(${PROJECT_NAME} main.cpp SDLFrontend.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_Core SDL3::SDL3-static)
