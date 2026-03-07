cmake_minimum_required(VERSION 4.0)

project (hello)

# compilers to use (> 9.2)

# openmp flags
#SET(OPENMP_CXX_FLAGS "-O3 -fopenmp --target=x86_64-pc-linux-gnu")
#SET(OPENMP_CXX_FLAGS "-O3 -fopenmp")
SET(OPENMP_CXX_FLAGS "-O3")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${OPENMP_CXX_FLAGS}" )


# app targets
add_executable(bakeryME bakeryME.cpp)

