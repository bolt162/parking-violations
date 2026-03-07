cmake_minimum_required(VERSION 2.8)

project (sharedmemory)

SET(CMAKE_CXX_FLAGS "-O3 -std=c++11")

# app targets
add_executable(shm-server shm-server.c)
add_executable(shm-client shm-client.c)
