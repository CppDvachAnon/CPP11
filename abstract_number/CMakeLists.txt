cmake_minimum_required(VERSION 3.8)
project(abstract_number)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -m64")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")

set(SOURCE_FILES main.cpp)
add_executable(abstract_number ${SOURCE_FILES})