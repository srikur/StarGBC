file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS *.cpp)
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)
set(GBMICROTEST_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GAMBATTE_TEXT ${CMAKE_CURRENT_SOURCE_DIR}/catalog/gambatte.json)
set(GAMBATTE_SCRIPT ${PARENT_DIR}/scripts/generate-catalog.py)
set(GAMBATTE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/GambatteTests.h)
set(GAMBATTE_GENERATED_HEADER ${GBMICROTEST_GENERATED_DIR}/GambatteTests.gen.h)

add_custom_command(
        OUTPUT ${GAMBATTE_GENERATED_HEADER}
        COMMAND ${Python3_EXECUTABLE} ${GAMBATTE_SCRIPT}
                ${GAMBATTE_TEXT} ${GAMBATTE_HEADER} ${GAMBATTE_GENERATED_HEADER}
        DEPENDS ${GAMBATTE_SCRIPT} ${GAMBATTE_TEXT} ${GAMBATTE_HEADER} ${PARENT_DIR}/includes/Model.h
        COMMENT "Generating GambatteTests.gen.h"
        VERBATIM)

set(CATALOG_GENERATED_HEADERS ${GBMICROTEST_GENERATED_DIR}/CatalogCases.inc ${GBMICROTEST_GENERATED_DIR}/MicrotestCases.inc)
set(ROM_CATALOGS ${CMAKE_CURRENT_SOURCE_DIR}/catalog/age.json ${CMAKE_CURRENT_SOURCE_DIR}/catalog/gbmicrotest.json ${CMAKE_CURRENT_SOURCE_DIR}/catalog/rom.json)
add_custom_command(
        OUTPUT ${CATALOG_GENERATED_HEADERS}
        COMMAND ${Python3_EXECUTABLE} ${PARENT_DIR}/scripts/generate-rom-catalog.py ${GBMICROTEST_GENERATED_DIR} ${ROM_CATALOGS}
        DEPENDS ${PARENT_DIR}/scripts/generate-rom-catalog.py ${ROM_CATALOGS} ${PARENT_DIR}/includes/Model.h
        VERBATIM)

add_subdirectory(mocks)
add_executable(${PROJECT_NAME}_Tests ${TEST_SOURCES} ${CATALOG_GENERATED_HEADERS} ${GAMBATTE_GENERATED_HEADER})
target_include_directories(${PROJECT_NAME}_Tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${GBMICROTEST_GENERATED_DIR})
target_link_libraries(${PROJECT_NAME}_Tests PRIVATE ${PROJECT_NAME}_Core StarGBC_Diagnostics doctest::doctest)

