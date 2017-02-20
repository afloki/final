cmake_minimum_required(VERSION 2.8)
project(http-server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

set (LIBS "-fopenmp")

set(SOURCE_EXE http.cpp)

add_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME} omp)
