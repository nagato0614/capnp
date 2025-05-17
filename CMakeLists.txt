cmake_minimum_required(VERSION 3.14)
project(capnproto_examples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CapnProto REQUIRED)
include_directories(${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})


# timer_example
add_executable(timer_example src/timer_example.cpp)
target_include_directories(timer_example
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(timer_example
        CapnProto::kj
        CapnProto::kj-async
)
