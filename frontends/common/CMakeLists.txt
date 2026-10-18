add_library(StarGBC_Host STATIC GameboyFiles.cpp)
target_include_directories(StarGBC_Host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(StarGBC_Host PUBLIC StarGBC_Core)
