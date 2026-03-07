cmake_minimum_required(VERSION 4.0)

project (hello)

# explicit compiler to use (> 9.2)
#set(CMAKE_C_COMPILER /opt/homebrew/Cellar/gcc/12.2.0/bin/gcc-12)
#set(CMAKE_CXX_COMPILER  /opt/homebrew/Cellar/gcc/12.2.0/bin/g++-12)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

# openmp flags
#SET(OPENMP_CXX_FLAGS "-O3 -fopenmp --target=x86_64-pc-linux-gnu")
SET(OPENMP_CXX_FLAGS "-O3 -fopenmp")
#SET(OPENMP_CXX_FLAGS "-O3")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${OPENMP_CXX_FLAGS}" )


# app targets
add_executable(hello hello-plain.cpp)
add_executable(hello-omp hello-omp.cpp)
add_executable(hello-omp2 hello-omp2.cpp)
add_executable(hello-cout hello-cout.cpp)

# advanced example
add_executable(hello-target hello-target.cpp)


