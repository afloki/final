cmake_minimum_required(VERSION 2.8)
project(http-server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
set(SOURCE_EXE http.cpp)

add_executable(final ${SOURCE_EXE})
target_link_libraries(${PROJECT_NAME} omp)
