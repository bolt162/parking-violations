# Library source files
set(LIB_SOURCES
    record.cpp
    parking.cpp
    csv_reader.cpp
    search.cpp
)

# Build the parking library
add_library(parking_lib ${LIB_SOURCES})

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
