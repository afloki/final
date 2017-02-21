cmake_minimum_required(VERSION 2.8)
project(final)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fopenmp")
set(SOURCE_FILES http.cpp)

file(MAKE_DIRECTORY /home/box/final)
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY /home/box/final )

add_executable(final  ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} omp)
