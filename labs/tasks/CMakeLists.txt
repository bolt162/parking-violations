cmake_minimum_required(VERSION 4.0)

project (tasks)

# openmp flags
SET(OPENMP_CXX_FLAGS "-O3 -fopenmp")
#SET(OPENMP_CXX_FLAGS "-O3 -fopenmp --target=x86_64-pc-linux-gnu")

SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${OPENMP_CXX_FLAGS}" )

# note we need to (should) include inlined classes in the compile line
add_executable(tasks tasks.cpp )

