cmake_minimum_required(VERSION 4.0)

project (leader)

# compilers to use (8.2)
#set(CMAKE_C_COMPILER gcc-12)
#set(CMAKE_CXX_COMPILER g++-12)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

# openmp flags
SET(OPENMP_CXX_FLAGS "-O3 -fopenmp")
#SET(OPENMP_CXX_FLAGS "-O3 -fopenmp --target=x86_64-pc-linux-gnu")

SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${OPENMP_CXX_FLAGS}" )

# note we need to (should) include inlined classes in the compile line
add_executable(leader task.hpp worker.hpp leader.cpp )

