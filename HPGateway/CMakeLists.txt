cmake_minimum_required(VERSION 3.10)
project(gateway)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
  -O2
  -g
  -Wall
  -Wextra
  -Wno-unused-parameter
  -fno-omit-frame-pointer
  -pthread
)

set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)

# spdlog
add_subdirectory(${THIRDPARTY_DIR}/spdlog)

# concurrentqueue (header only)
include_directories(${THIRDPARTY_DIR}/concurrentqueue)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

set(NET_SRC
  src/net/Acceptor.cpp
  src/net/Buffer.cpp
  src/net/Channel.cpp
  src/net/EventLoop.cpp
  src/net/EventLoopThread.cpp
  src/net/EventLoopThreadPool.cpp
  src/net/Poller.cpp
  src/net/SocketUtil.cpp
  src/net/TcpConnection.cpp
  src/net/TcpServer.cpp
)

set(BASE_SRC
  src/base/Logger.cpp
  src/base/Thread.cpp
  src/base/ThreadPool.cpp
  src/base/Timestamp.cpp
)

set(PROTOCOL_SRC
  src/protocol/Codec.cpp
  src/protocol/PacketDispatcher.cpp
)

set(GATEWAY_SRC
  src/gateway/ConnectionManager.cpp
  src/gateway/GatewayApiImpl.cpp
  src/gateway/GatewayContext.cpp
  src/gateway/GatewayServer.cpp
  src/gateway/SessionManager.cpp
)

set(BUSINESS_SRC
  src/business/MessageRouter.cpp
  src/business/WorkerPool.cpp
)

set(MAIN_SRC
  src/main.cpp
)

set(API_CLI_SRC
  src/api_cli/ApiClient.cpp
  src/api_cli/main.cpp
)

add_executable(gateway
  ${NET_SRC}
  ${BASE_SRC}
  ${PROTOCOL_SRC}
  ${GATEWAY_SRC}
  ${BUSINESS_SRC}
  ${MAIN_SRC}
)

add_executable(api_cli
  ${API_CLI_SRC}
)

target_link_libraries(gateway spdlog::spdlog)

target_link_libraries(gateway pthread)

target_link_libraries(api_cli pthread)
