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
