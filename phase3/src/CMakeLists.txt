# Phase 3 SoA library
add_library(parking_soa_lib
    record.cpp
    soa_store.cpp
    soa_csv_reader.cpp
    soa_search.cpp
    parking.cpp
)
 
# Demo application
add_executable(demo apps/demo.cpp)
target_link_libraries(demo parking_soa_lib)

# Load benchmark
add_executable(load_benchmark apps/load_benchmark.cpp)
target_link_libraries(load_benchmark parking_soa_lib)

# Search benchmark
add_executable(search_benchmark apps/search_benchmark.cpp)
target_link_libraries(search_benchmark parking_soa_lib)
