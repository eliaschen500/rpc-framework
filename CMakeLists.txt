cmake_minimum_required(VERSION 3.16)
project(rpc_framework CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build options ──────────────────────────────────────────────────────────
option(WITH_ZOOKEEPER "Link against the ZooKeeper C client" OFF)

# ── Find Protobuf ──────────────────────────────────────────────────────────
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

# ── Generate proto sources ─────────────────────────────────────────────────
set(PROTO_OUT_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

function(compile_proto PROTO_FILE OUT_SRCS OUT_HDRS)
    get_filename_component(ABS_PROTO ${PROTO_FILE} ABSOLUTE)
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    get_filename_component(PROTO_DIR  ${ABS_PROTO}  DIRECTORY)

    set(GENERATED_SRC ${PROTO_OUT_DIR}/${PROTO_NAME}.pb.cc)
    set(GENERATED_HDR ${PROTO_OUT_DIR}/${PROTO_NAME}.pb.h)

    add_custom_command(
        OUTPUT  ${GENERATED_SRC} ${GENERATED_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                --cpp_out=${PROTO_OUT_DIR}
                -I${PROTO_DIR}
                ${ABS_PROTO}
        DEPENDS ${ABS_PROTO}
        COMMENT "Generating protobuf for ${PROTO_FILE}"
    )
    set(${OUT_SRCS} ${GENERATED_SRC} PARENT_SCOPE)
    set(${OUT_HDRS} ${GENERATED_HDR} PARENT_SCOPE)
endfunction()

# Core RPC proto.
compile_proto(proto/rpc.proto RPC_PROTO_SRC RPC_PROTO_HDR)

# ── Core library ───────────────────────────────────────────────────────────
set(LIB_SOURCES
    src/net/buffer.cpp
    src/net/channel.cpp
    src/net/epoll_poller.cpp
    src/net/event_loop.cpp
    src/net/tcp_connection.cpp
    src/net/tcp_server.cpp
    src/net/thread_pool.cpp
    src/codec/codec.cpp
    src/rpc/rpc_server.cpp
    src/rpc/rpc_channel.cpp
    src/registry/load_balancer.cpp
    src/registry/zookeeper_client.cpp
    src/registry/service_registry.cpp
    ${RPC_PROTO_SRC}
)

add_library(rpc_core STATIC ${LIB_SOURCES})
target_include_directories(rpc_core PUBLIC
    include
    ${PROTO_OUT_DIR}
)
target_link_libraries(rpc_core PUBLIC
    ${Protobuf_LIBRARIES}
    pthread
)

if(WITH_ZOOKEEPER)
    target_compile_definitions(rpc_core PUBLIC WITH_ZOOKEEPER)
    target_link_libraries(rpc_core PUBLIC zookeeper_mt)
endif()

# ── Examples ───────────────────────────────────────────────────────────────
add_subdirectory(example)
