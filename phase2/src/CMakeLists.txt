# Library source files
set(LIB_SOURCES
    core/violation_record.cpp
    io/csv_parser.cpp
    io/csv_reader.cpp
    io/file_loader.cpp
    data/data_store.cpp
    search/linear_search.cpp
    search/indexed_search.cpp
    search/parallel_search.cpp
    api/parking_api.cpp
)

# Build the parking library
add_library(parking_lib ${LIB_SOURCES})
target_link_libraries(parking_lib PUBLIC OpenMP::OpenMP_CXX)

# Dependency variable for linking
set(BUILD_LIBRARIES parking_lib PARENT_SCOPE)

# Quick integration test: load a CSV and print stats
add_executable(demo apps/demo.cpp)
target_link_libraries(demo parking_lib)

# Benchmark executables
# Benchmark sources include headers from test/ for benchmark_harness.hpp
include_directories(${CMAKE_SOURCE_DIR}/test)

add_executable(load_benchmark apps/load_benchmark.cpp)
target_link_libraries(load_benchmark parking_lib)

add_executable(search_benchmark apps/search_benchmark.cpp)
target_link_libraries(search_benchmark parking_lib)
