add_library(${PROJECT_NAME}_Diagnostics STATIC src/Contracts.cpp src/Hash.cpp src/Report.cpp src/Artifacts.cpp src/Trace.cpp src/Watchpoints.cpp src/Checkpoint.cpp src/Replay.cpp src/Provenance.cpp src/Comparison.cpp src/Experiments.cpp)
target_include_directories(${PROJECT_NAME}_Diagnostics PUBLIC include)
target_link_libraries(${PROJECT_NAME}_Diagnostics PUBLIC ${PROJECT_NAME}_Core ${PROJECT_NAME}_Host)

add_executable(${PROJECT_NAME}_Debug tools/Debug.cpp)
target_link_libraries(${PROJECT_NAME}_Debug PRIVATE starparse::starparse ${PROJECT_NAME}_Core ${PROJECT_NAME}_Diagnostics)

add_executable(${PROJECT_NAME}_Experiment tools/Experiment.cpp)
target_link_libraries(${PROJECT_NAME}_Experiment PRIVATE starparse::starparse ${PROJECT_NAME}_Diagnostics)
