cmake_minimum_required(VERSION 3.14)
project(capnproto_examples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CapnProto REQUIRED)
include_directories(${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})

# ターゲットを登録する関数
function(add_example name)
    add_executable(${name} src/${name}.cpp)
    target_include_directories(${name}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${name}
            CapnProto::kj
            CapnProto::kj-async
            CapnProto::capnp
            CapnProto::capnpc
    )
endfunction(add_example)



# timer_example
add_example(timer_example)
