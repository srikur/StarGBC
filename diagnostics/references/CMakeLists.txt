option(STARGBC_REFERENCE_ADAPTERS "Build optional pinned Gambatte and SameBoy diagnostic adapters" OFF)

add_executable(StarGBC_Reference Driver.cpp StarGBC.cpp)
target_link_libraries(StarGBC_Reference PRIVATE StarGBC_Diagnostics)

if (STARGBC_REFERENCE_ADAPTERS)
    enable_language(C)
    FetchContent_Declare(stargbc_gambatte
            URL https://codeload.github.com/pokemon-speedrunning/gambatte-core/tar.gz/5a41a68c25402421fb1983ddadc9faf2418ddb0f
            URL_HASH SHA256=62711f8b42873da683e60cb984217805ef93fb60e5fbab6c291e8985d1ed0e97)
    FetchContent_Declare(stargbc_sameboy
            URL https://codeload.github.com/LIJI32/SameBoy/tar.gz/c71f110eb1b1e87914fddd9c0e68712dddce787d
            URL_HASH SHA256=a6cd8571a33dfc9633b0d820f863d8d765f099a2e265f64f1a3b6c578c6d76f5)
    FetchContent_MakeAvailable(stargbc_gambatte stargbc_sameboy)

    file(GLOB_RECURSE GAMBATTE_SOURCES "${stargbc_gambatte_SOURCE_DIR}/libgambatte/src/*.cpp")
    list(FILTER GAMBATTE_SOURCES EXCLUDE REGEX "/(cinterface|file_zip)\\.cpp$")
    add_library(StarGBC_GambatteCore STATIC ${GAMBATTE_SOURCES})
    set_target_properties(StarGBC_GambatteCore PROPERTIES CXX_STANDARD 11 COMPILE_OPTIONS "-w")
    target_compile_definitions(StarGBC_GambatteCore PUBLIC HAVE_STDINT_H)
    target_include_directories(StarGBC_GambatteCore SYSTEM PUBLIC "${stargbc_gambatte_SOURCE_DIR}/libgambatte/include"
            PRIVATE "${stargbc_gambatte_SOURCE_DIR}/libgambatte/src" "${stargbc_gambatte_SOURCE_DIR}/common")
    add_executable(StarGBC_GambatteReference Driver.cpp Gambatte.cpp)
    target_link_libraries(StarGBC_GambatteReference PRIVATE StarGBC_Diagnostics StarGBC_GambatteCore)

    file(GLOB SAMEBOY_SOURCES "${stargbc_sameboy_SOURCE_DIR}/Core/*.c")
    list(FILTER SAMEBOY_SOURCES EXCLUDE REGEX "/(cheats|cheat_search|debugger|rewind|symbol_hash|sm83_disassembler)\\.c$")
    add_library(StarGBC_SameBoyCore STATIC ${SAMEBOY_SOURCES})
    set_target_properties(StarGBC_SameBoyCore PROPERTIES C_STANDARD 11 C_EXTENSIONS ON COMPILE_OPTIONS "-w" INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    target_compile_definitions(StarGBC_SameBoyCore PRIVATE GB_INTERNAL GB_VERSION="pinned-diagnostics"
            PUBLIC GB_DISABLE_TIMEKEEPING GB_DISABLE_REWIND GB_DISABLE_DEBUGGER GB_DISABLE_CHEATS GB_DISABLE_CHEAT_SEARCH)
    target_include_directories(StarGBC_SameBoyCore SYSTEM PUBLIC "${stargbc_sameboy_SOURCE_DIR}")
    add_library(StarGBC_SameBoyClock STATIC SameBoyClock.c)
    set_target_properties(StarGBC_SameBoyClock PROPERTIES C_STANDARD 11 C_EXTENSIONS ON COMPILE_OPTIONS "-Wall;-Werror" INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    target_link_libraries(StarGBC_SameBoyClock PRIVATE StarGBC_SameBoyCore)
    add_executable(StarGBC_SameBoyReference Driver.cpp SameBoy.cpp)
    target_link_libraries(StarGBC_SameBoyReference PRIVATE StarGBC_Diagnostics StarGBC_SameBoyCore StarGBC_SameBoyClock)
endif ()
