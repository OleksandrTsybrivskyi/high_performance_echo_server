cmake_minimum_required(VERSION 3.10)

project(high_performance_echo_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(high_performance_echo_server src/main.cpp)

target_include_directories(high_performance_echo_server PRIVATE include)

if(WIN32)
    target_link_libraries(high_performance_echo_server PRIVATE ws2_32)
elseif(UNIX)
    target_compile_definitions(high_performance_echo_server PRIVATE LINUX)
endif()