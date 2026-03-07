cmake_minimum_required(VERSION 3.3)

project (hello)

# compilers to use (> 9.2)

# openmp flags
#SET(OPENMP_CXX_FLAGS "-O3 -fopenmp --target=x86_64-pc-linux-gnu")
SET(OPENMP_CXX_FLAGS "-O3 -fopenmp")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${OPENMP_CXX_FLAGS}" )


# app targets
add_executable(dijkstraME dijkstraME.cpp)
add_executable(dijkstraME2 dijkstraME-v2.cpp)
add_executable(dijkstraME-omp dijkstraME-omp.cpp)

