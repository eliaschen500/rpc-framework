cmake_minimum_required(VERSION 3.16)

# ── Hello example ──────────────────────────────────────────────────────────
compile_proto(${CMAKE_CURRENT_SOURCE_DIR}/hello/hello.proto
              HELLO_PROTO_SRC HELLO_PROTO_HDR)

add_executable(hello_server
    hello/hello_server.cpp
    ${HELLO_PROTO_SRC}
)
target_include_directories(hello_server PRIVATE ${PROTO_OUT_DIR})
target_link_libraries(hello_server PRIVATE rpc_core)

add_executable(hello_client
    hello/hello_client.cpp
    ${HELLO_PROTO_SRC}
)
target_include_directories(hello_client PRIVATE ${PROTO_OUT_DIR})
target_link_libraries(hello_client PRIVATE rpc_core)

# ── Calculator example ────────────────────────────────────────────────────
compile_proto(${CMAKE_CURRENT_SOURCE_DIR}/calculator/calculator.proto
              CALC_PROTO_SRC CALC_PROTO_HDR)

add_executable(calc_server
    calculator/calc_server.cpp
    ${CALC_PROTO_SRC}
)
target_include_directories(calc_server PRIVATE ${PROTO_OUT_DIR})
target_link_libraries(calc_server PRIVATE rpc_core)

add_executable(calc_client
    calculator/calc_client.cpp
    ${CALC_PROTO_SRC}
)
target_include_directories(calc_client PRIVATE ${PROTO_OUT_DIR})
target_link_libraries(calc_client PRIVATE rpc_core)
