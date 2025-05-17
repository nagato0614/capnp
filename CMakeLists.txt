cmake_minimum_required(VERSION 3.14)
project(RepeatingTimerExample)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CapnProto REQUIRED)
include_directories(${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})

add_executable(timer_example src/main.cpp)

target_link_libraries(timer_example
        CapnProto::kj
        CapnProto::kj-async
)
