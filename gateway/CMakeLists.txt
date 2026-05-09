cmake_minimum_required(VERSION 3.12)
project(GatewayDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")

# ---- 1. 找到 Protobuf 库（用于链接） ----
set(Protobuf_DIR "/usr/local/Cellar/protobuf/34.1/lib/cmake/protobuf")
find_package(Protobuf CONFIG REQUIRED)

# ---- 1.5 找到 ZeroMQ 库 ----
set(ZeroMQ_DIR "/usr/local/Cellar/zeromq/4.3.5_2/lib/cmake")
find_package(ZeroMQ CONFIG REQUIRED)

# ---- 2. 找到 protoc 编译器（用于生成 .pb.cc/.h） ----
# protoc 一般已通过 Homebrew link 到 /usr/local/bin，直接查找即可
find_program(PROTOC_EXE protoc
  HINTS "/usr/local/Cellar/protobuf/34.1/bin")
if(NOT PROTOC_EXE)
  message(FATAL_ERROR "protoc not found")
endif()

# ---- 3. 指定 proto 文件及生成目录 ----
set(PROTO_FILE ${CMAKE_SOURCE_DIR}/proto/gateway.proto)
set(PROTO_GEN_DIR ${CMAKE_SOURCE_DIR}/proto/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# ---- 4. 自定义命令：调用 protoc 生成 C++ 文件 ----
add_custom_command(
  OUTPUT ${PROTO_GEN_DIR}/gateway.pb.cc
  ${PROTO_GEN_DIR}/gateway.pb.h
  COMMAND ${PROTOC_EXE}
  --proto_path=${CMAKE_SOURCE_DIR}/proto
  --cpp_out=${PROTO_GEN_DIR}
  ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  COMMENT "Generating gateway.pb.cc/h from gateway.proto"
)

# ---- 5. 将生成的 protobuf 源文件打包成库（避免重复编译） ----
add_library(proto_gen STATIC ${PROTO_GEN_DIR}/gateway.pb.cc)
target_include_directories(proto_gen PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(proto_gen PUBLIC protobuf::libprotobuf)

# ---- 6. 消息工具库（ZeroMQ 收发包，不依赖 protobuf） ----
add_library(message_utils STATIC common/message_utils.cpp)
target_include_directories(message_utils PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(message_utils PUBLIC libzmq)

# ---- 6.5 线程池（通用任务调度） ----
add_library(thread_pool STATIC common/ThreadPool.cpp)
target_include_directories(thread_pool PUBLIC ${CMAKE_SOURCE_DIR}/common)

# ---- 7. Gateway API 库（SDK，对外暴露 GatewayApi/GatewaySpi，内部隐藏实现） ----
add_library(gateway_api STATIC api/GatewayApiImpl.cpp)
target_include_directories(gateway_api PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${PROTO_GEN_DIR}
)
target_link_libraries(gateway_api PUBLIC
  message_utils
  proto_gen
)

# ---- 8. 服务器（Gateway 框架 + AuthHandler 业务注入） ----
add_executable(server
  server/server.cpp
  server/GatewayImpl.cpp
  server/AuthHandler.cpp
)
target_include_directories(server PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${PROTO_GEN_DIR}
)
target_link_libraries(server PRIVATE
  message_utils
  proto_gen
  thread_pool
)

# ---- 9. 客户端（仅通过 GatewayApi/GatewaySpi 与网关交互） ----
add_executable(client client/client.cpp)
target_include_directories(client PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${PROTO_GEN_DIR}
)
target_link_libraries(client PRIVATE
  gateway_api
)