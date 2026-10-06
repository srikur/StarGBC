FetchContent_Declare(stargbc_lua_source
    URL https://www.lua.org/ftp/lua-5.5.1.tar.gz
    URL_HASH SHA256=1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce
    EXCLUDE_FROM_ALL SYSTEM)
FetchContent_MakeAvailable(stargbc_lua_source)
set(lua_sources
    lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c
    lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c
    ltm.c lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c
    ldblib.c liolib.c lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c
    lutf8lib.c linit.c)
list(TRANSFORM lua_sources PREPEND "${stargbc_lua_source_SOURCE_DIR}/src/")
set_source_files_properties(${lua_sources} PROPERTIES LANGUAGE CXX)
add_library(stargbc_lua STATIC ${lua_sources})
target_include_directories(stargbc_lua SYSTEM PUBLIC "${stargbc_lua_source_SOURCE_DIR}/src")
set_target_properties(stargbc_lua PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    target_compile_definitions(stargbc_lua PRIVATE _XOPEN_SOURCE=0)
endif()
