cmake_minimum_required(VERSION 3.14)
project(capnproto_examples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CapnProto REQUIRED)
include_directories(${CAPNP_INCLUDE_DIRS})
add_definitions(${CAPNP_DEFINITIONS})

# Cap'n Proto スキーマをコンパイル
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS
        schema/notification.capnp
)

message(STATUS "Cap'n Proto generated sources: ${CAPNP_SRCS}")
message(STATUS "Cap'n Proto generated headers: ${CAPNP_HDRS}")

# ターゲットを登録する関数
function(add_example name)
    message(STATUS "Adding example: ${name}")
    add_executable(${name} src/${name}.cpp ${CAPNP_SRCS} ${CAPNP_HDRS})
    target_include_directories(${name}
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/schema
    )
    target_link_libraries(${name}
            CapnProto::kj
            CapnProto::kj-async
            CapnProto::capnp
            CapnProto::capnp-rpc
    )
endfunction(add_example)


add_example(timer_example)
add_example(notifier_client_example)
add_example(notifier_server_example)
add_example(temp_lib)
add_example(executer_example)
add_example(delay_example)