add_library(StarGBC_Diagnostics STATIC src/Contracts.cpp src/Hash.cpp src/Report.cpp src/Artifacts.cpp src/Trace.cpp src/Watchpoints.cpp src/Checkpoint.cpp src/Replay.cpp src/Provenance.cpp src/Comparison.cpp src/Experiments.cpp)
target_include_directories(StarGBC_Diagnostics PUBLIC include)
target_link_libraries(StarGBC_Diagnostics PUBLIC StarGBC_Core)

add_executable(StarGBC_Debug tools/Debug.cpp)
target_link_libraries(StarGBC_Debug PRIVATE StarGBC_Core StarGBC_Diagnostics)

add_executable(StarGBC_Experiment tools/Experiment.cpp)
target_link_libraries(StarGBC_Experiment PRIVATE StarGBC_Diagnostics)
