add_library(StarGBC_Diagnostics STATIC src/Contracts.cpp src/Hash.cpp src/Report.cpp src/Artifacts.cpp src/Trace.cpp)
target_include_directories(StarGBC_Diagnostics PUBLIC include)

add_executable(StarGBC_Debug tools/Debug.cpp)
target_link_libraries(StarGBC_Debug PRIVATE StarGBC_Core StarGBC_Diagnostics)
