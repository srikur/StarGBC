add_library(StarGBC_Diagnostics STATIC src/Contracts.cpp src/Hash.cpp src/Report.cpp src/Artifacts.cpp src/Trace.cpp src/Watchpoints.cpp src/Checkpoint.cpp src/Replay.cpp src/Provenance.cpp)
target_include_directories(StarGBC_Diagnostics PUBLIC include)
target_link_libraries(StarGBC_Diagnostics PUBLIC StarGBC_Core)

add_executable(StarGBC_Debug tools/Debug.cpp)
target_link_libraries(StarGBC_Debug PRIVATE StarGBC_Core StarGBC_Diagnostics)
