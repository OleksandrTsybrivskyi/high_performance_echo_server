cmake_minimum_required(VERSION 3.10)

project(MyProject)

set(CMAKE_CXX_STANDART 17)
set(CMAKE_CXX_STANDART_REQUIRED ON)

add_executable(MyProject src/main.cpp)

target_include_directories(MyProject PRIVATE include)
