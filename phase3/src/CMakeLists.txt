# Phase 3 SoA library
add_library(parking_soa_lib
    record.cpp
    soa_store.cpp
    soa_csv_reader.cpp
    soa_search.cpp
    parking.cpp
)

# Dependency variable for linking
set(BUILD_LIBRARIES parking_soa_lib PARENT_SCOPE)
