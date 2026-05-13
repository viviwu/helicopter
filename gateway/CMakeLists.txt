cmake_minimum_required(VERSION 3.12)
project(GatewayDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")

# ---- 1. Dependencies ----
set(Protobuf_DIR "/usr/local/Cellar/protobuf/34.1/lib/cmake/protobuf")
find_package(Protobuf CONFIG REQUIRED)

set(ZeroMQ_DIR "/usr/local/Cellar/zeromq/4.3.5_2/lib/cmake")
find_package(ZeroMQ CONFIG REQUIRED)

find_program(PROTOC_EXE protoc
  HINTS "/usr/local/Cellar/protobuf/34.1/bin")
if(NOT PROTOC_EXE)
  message(FATAL_ERROR "protoc not found")
endif()

# ---- 2. Proto generation ----
set(PROTO_GEN_DIR ${CMAKE_SOURCE_DIR}/proto/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# Trade proto
set(TRADE_PROTO_FILE ${CMAKE_SOURCE_DIR}/trade/trade.proto)
set(TRADE_PROTO_GEN_DIR ${CMAKE_SOURCE_DIR}/trade/generated)
file(MAKE_DIRECTORY ${TRADE_PROTO_GEN_DIR})
add_custom_command(
  OUTPUT ${TRADE_PROTO_GEN_DIR}/trade.pb.cc ${TRADE_PROTO_GEN_DIR}/trade.pb.h
  COMMAND ${PROTOC_EXE}
    --proto_path=${CMAKE_SOURCE_DIR}/trade
    --cpp_out=${TRADE_PROTO_GEN_DIR}
    ${TRADE_PROTO_FILE}
  DEPENDS ${TRADE_PROTO_FILE}
  COMMENT "Generating trade.pb.cc/h from trade.proto"
)

# Quote proto
set(QUOTE_PROTO_FILE ${CMAKE_SOURCE_DIR}/quote/quote.proto)
set(QUOTE_PROTO_GEN_DIR ${CMAKE_SOURCE_DIR}/quote/generated)
file(MAKE_DIRECTORY ${QUOTE_PROTO_GEN_DIR})
add_custom_command(
  OUTPUT ${QUOTE_PROTO_GEN_DIR}/quote.pb.cc ${QUOTE_PROTO_GEN_DIR}/quote.pb.h
  COMMAND ${PROTOC_EXE}
    --proto_path=${CMAKE_SOURCE_DIR}/quote
    --cpp_out=${QUOTE_PROTO_GEN_DIR}
    ${QUOTE_PROTO_FILE}
  DEPENDS ${QUOTE_PROTO_FILE}
  COMMENT "Generating quote.pb.cc/h from quote.proto"
)

# ---- 3. Proto libraries ----
add_library(trade_proto_gen STATIC ${TRADE_PROTO_GEN_DIR}/trade.pb.cc)
target_include_directories(trade_proto_gen PUBLIC ${TRADE_PROTO_GEN_DIR})
target_link_libraries(trade_proto_gen PUBLIC protobuf::libprotobuf)

add_library(quote_proto_gen STATIC ${QUOTE_PROTO_GEN_DIR}/quote.pb.cc)
target_include_directories(quote_proto_gen PUBLIC ${QUOTE_PROTO_GEN_DIR})
target_link_libraries(quote_proto_gen PUBLIC protobuf::libprotobuf)

# ---- 4. Shared utilities ----
add_library(message_utils STATIC common/message_utils.cpp)
target_include_directories(message_utils PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(message_utils PUBLIC libzmq)

add_library(thread_pool STATIC common/ThreadPool.cpp)
target_include_directories(thread_pool PUBLIC ${CMAKE_SOURCE_DIR}/common)

# ---- 5. Framework ----
add_library(gateway_framework STATIC
  framework/RouterServer.cpp
  framework/PubServer.cpp
  framework/GatewayApi.cpp
)
target_include_directories(gateway_framework PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(gateway_framework PUBLIC message_utils thread_pool)

# ---- 6. Trade Gateway ----
# TradeGatewayServer (server-side)
add_library(trade_server STATIC trade/TradeGatewayServer.cpp)
target_include_directories(trade_server PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(trade_server PUBLIC gateway_framework trade_proto_gen)

# TradeApi (client-side SDK)
add_library(trade_api STATIC trade/TradeApi.cpp)
target_include_directories(trade_api PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(trade_api PUBLIC gateway_framework trade_proto_gen)

# ---- 7. Quote Gateway ----
# QuoteGatewayServer (server-side)
add_library(quote_server STATIC quote/QuoteGatewayServer.cpp)
target_include_directories(quote_server PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(quote_server PUBLIC gateway_framework quote_proto_gen)

# QuoteApi (client-side SDK)
add_library(quote_api STATIC quote/QuoteApi.cpp)
target_include_directories(quote_api PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(quote_api PUBLIC gateway_framework quote_proto_gen)

# ---- 8. Server executable ----
add_executable(server server/server.cpp)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(server PRIVATE trade_server quote_server)

# ---- 9. Client executable ----
add_executable(client client/client.cpp)
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(client PRIVATE trade_api quote_api)
