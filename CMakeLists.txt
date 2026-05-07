cmake_minimum_required(VERSION 3.10)

project(high_performance_echo_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Force Release Build (Crucial for performance)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(high_performance_echo_server src/main.cpp)

target_include_directories(high_performance_echo_server PRIVATE include)

# 2. Add Threading Support (Required for our new multithreaded code)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 3. Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(high_performance_echo_server PRIVATE
        -O3             # Maximum speed optimization
        -march=native   # Optimize specifically for the CPU architecture compiling the code
    )
endif()

# 4. Enable Link Time Optimization (LTO)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
    set_property(TARGET high_performance_echo_server PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(WIN32)
    target_link_libraries(high_performance_echo_server PRIVATE ws2_32)
elseif(UNIX)
    target_compile_definitions(high_performance_echo_server PRIVATE LINUX)
    # Link the pthread library explicitly
    target_link_libraries(high_performance_echo_server PRIVATE Threads::Threads)
endif()