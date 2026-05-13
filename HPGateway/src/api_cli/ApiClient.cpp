/**
 **************************************************************************
 * @file    :ApiClient.cpp
 * @author  :viviwu
 * @brief   :Gateway API客户端实现
 * @version :0.1
 * @date    :5/12/26
 * **************************************************************************
 */

#include "ApiClient.h"
#include "../protocol/Packet.h"
#include "gateway/GatewayStruct.h"
#include "gateway/GatewayError.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>

namespace gateway {

// ---- ApiClient ----

ApiClient::ApiClient()
    : sockfd_(-1), seq_(1) {
  recvBuf_.reserve(64 * 1024);
}

ApiClient::~ApiClient() {
  Disconnect();
}

bool ApiClient::Connect(const std::string& host, uint16_t port) {
  if (sockfd_ >= 0) Disconnect();

  struct addrinfo hints, *result = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char portStr[16];
  snprintf(portStr, sizeof(portStr), "%u", port);

  int ret = ::getaddrinfo(host.c_str(), portStr, &hints, &result);
  if (ret != 0) {
    fprintf(stderr, "[ApiClient] getaddrinfo: %s\n", gai_strerror(ret));
    return false;
  }

  sockfd_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sockfd_ < 0) {
    perror("[ApiClient] socket");
    ::freeaddrinfo(result);
    return false;
  }

  // 设置非阻塞连接
  int flags = ::fcntl(sockfd_, F_GETFL, 0);
  ::fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);

  ret = ::connect(sockfd_, result->ai_addr, result->ai_addrlen);
  ::freeaddrinfo(result);

  if (ret < 0 && errno != EINPROGRESS) {
    perror("[ApiClient] connect");
    ::close(sockfd_);
    sockfd_ = -1;
    return false;
  }

  if (ret < 0) {
    // 等待连接完成
    struct pollfd pfd;
    pfd.fd = sockfd_;
    pfd.events = POLLOUT;
    ret = ::poll(&pfd, 1, 3000);
    if (ret <= 0 || !(pfd.revents & POLLOUT)) {
      fprintf(stderr, "[ApiClient] connect timeout to %s:%u\n", host.c_str(), port);
      ::close(sockfd_);
      sockfd_ = -1;
      return false;
    }
    // 检查 socket 错误
    int sockErr = 0;
    socklen_t len = sizeof(sockErr);
    ::getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &sockErr, &len);
    if (sockErr != 0) {
      fprintf(stderr, "[ApiClient] connect error: %s\n", strerror(sockErr));
      ::close(sockfd_);
      sockfd_ = -1;
      return false;
    }
  }

  printf("[ApiClient] Connected to %s:%u\n", host.c_str(), port);
  return true;
}

void ApiClient::Disconnect() {
  if (sockfd_ >= 0) {
    ::close(sockfd_);
    sockfd_ = -1;
  }
  recvBuf_.clear();
}

uint32_t ApiClient::SendRequest(uint16_t cmd, const void* data, size_t len) {
  if (sockfd_ < 0) return 0;

  uint32_t seq = seq_++;

  // 编码报文: [length(4)][cmd(2)][flag(2)][seq(4)][body(N)]
  uint32_t totalLen = PacketHeader::kHeaderLength + static_cast<uint32_t>(len);
  char header[PacketHeader::kHeaderLength];
  memcpy(header, &totalLen, 4);
  memcpy(header + 4, &cmd, 2);
  uint16_t flag = 0;
  memcpy(header + 6, &flag, 2);
  memcpy(header + 8, &seq, 4);

  // 发送 header + body
  struct iovec iov[2];
  iov[0].iov_base = header;
  iov[0].iov_len = sizeof(header);
  iov[1].iov_base = const_cast<void*>(data);
  iov[1].iov_len = len;

  ssize_t n = ::writev(sockfd_, iov, 2);
  if (n < 0) {
    perror("[ApiClient] writev");
    return 0;
  }

  printf("[ApiClient] Sent cmd=0x%04X seq=%u totalLen=%u\n", cmd, seq, totalLen);
  return seq;
}

void ApiClient::SendHeartbeatRsp(uint32_t seq) {
  if (sockfd_ < 0) return;

  uint32_t totalLen = PacketHeader::kHeaderLength;
  char header[PacketHeader::kHeaderLength];
  memcpy(header, &totalLen, 4);
  uint16_t cmd = kCmdHeartbeatRsp;
  memcpy(header + 4, &cmd, 2);
  uint16_t flag = 0;
  memcpy(header + 6, &flag, 2);
  memcpy(header + 8, &seq, 4);

  ssize_t n = ::write(sockfd_, header, sizeof(header));
  if (n > 0) {
    printf("[ApiClient] Sent HeartbeatRsp seq=%u\n", seq);
  }
}

bool ApiClient::RecvAll(void* buf, size_t len) {
  size_t received = 0;
  char* ptr = static_cast<char*>(buf);
  while (received < len) {
    ssize_t n = ::recv(sockfd_, ptr + received, len - received, 0);
    if (n <= 0) {
      if (n == 0) {
        fprintf(stderr, "[ApiClient] Connection closed by peer\n");
      } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        perror("[ApiClient] recv");
      }
      return false;
    }
    received += static_cast<size_t>(n);
  }
  return true;
}

bool ApiClient::RecvPacket(int timeoutMs) {
  if (sockfd_ < 0) return false;

  // 先读取包头（12 字节）
  PacketHeader header;
  if (!RecvAll(&header, sizeof(header))) {
    return false;
  }

  if (header.length < PacketHeader::kHeaderLength || header.length > 64 * 1024) {
    fprintf(stderr, "[ApiClient] Invalid packet length: %u\n", header.length);
    return false;
  }

  uint32_t bodyLen = header.length - PacketHeader::kHeaderLength;
  std::vector<char> body(bodyLen);
  if (bodyLen > 0) {
    if (!RecvAll(body.data(), bodyLen)) {
      return false;
    }
  }

  // 打印响应
  const char* cmdName = "Unknown";
  switch (header.cmd) {
    case kCmdLoginReq:        cmdName = "LoginReq"; break;
    case kCmdLoginRsp:        cmdName = "LoginRsp"; break;
    case kCmdLogoutReq:       cmdName = "LogoutReq"; break;
    case kCmdLogoutRsp:       cmdName = "LogoutRsp"; break;
    case kCmdOrderInsertReq:  cmdName = "OrderInsertReq"; break;
    case kCmdOrderInsertRsp:  cmdName = "OrderInsertRsp"; break;
    case kCmdOrderCancelReq:  cmdName = "OrderCancelReq"; break;
    case kCmdOrderCancelRsp:  cmdName = "OrderCancelRsp"; break;
    case kCmdSubscribeReq:    cmdName = "SubscribeReq"; break;
    case kCmdSubscribeRsp:    cmdName = "SubscribeRsp"; break;
    case kCmdUnsubscribeReq:  cmdName = "UnsubscribeReq"; break;
    case kCmdUnsubscribeRsp:  cmdName = "UnsubscribeRsp"; break;
    case kCmdTickNotify:      cmdName = "TickNotify"; break;
    case kCmdOrderNotify:     cmdName = "OrderNotify"; break;
    case kCmdTradeNotify:     cmdName = "TradeNotify"; break;
    case kCmdHeartbeatReq:    cmdName = "HeartbeatReq"; break;
    case kCmdHeartbeatRsp:    cmdName = "HeartbeatRsp"; break;
  }

  printf("\n=== Response: %s (0x%04X) seq=%u len=%u ===\n",
         cmdName, header.cmd, header.seq, header.length);

  // 解析并打印 body
  if (bodyLen > 0) {
    switch (header.cmd) {
      case kCmdLoginRsp: {
        if (bodyLen >= sizeof(LoginResponse)) {
          auto* rsp = reinterpret_cast<const LoginResponse*>(body.data());
          printf("  tradingDay: %d\n", rsp->tradingDay);
          printf("  account:    %.*s\n", ACCOUNT_LEN, rsp->account);
          printf("  balance:    %.2f\n", rsp->balance);
          printf("  available:  %.2f\n", rsp->available);
          printf("  frontId:    %d\n", rsp->frontId);
          printf("  sessionId:  %d\n", rsp->sessionId);
        }
        break;
      }
      case kCmdOrderInsertRsp: {
        if (bodyLen >= sizeof(OrderInfo)) {
          auto* info = reinterpret_cast<const OrderInfo*>(body.data());
          printf("  orderId:       %.*s\n", ORDER_ID_LEN, info->orderId);
          printf("  clientOrderId: %.*s\n", ORDER_ID_LEN, info->clientOrderId);
          printf("  symbol:        %.*s\n", SYMBOL_LEN, info->symbol);
          printf("  exchange:      %.*s\n", EXCHANGE_LEN, info->exchange);
          printf("  direction:     %d\n", info->direction);
          printf("  offset:        %d\n", info->offset);
          printf("  price:         %.2f\n", info->price);
          printf("  volume:        %d\n", info->volume);
          printf("  tradedVolume:  %d\n", info->tradedVolume);
          printf("  status:        %d\n", info->status);
        }
        break;
      }
      case kCmdOrderCancelRsp: {
        if (bodyLen >= sizeof(OrderInfo)) {
          auto* info = reinterpret_cast<const OrderInfo*>(body.data());
          printf("  orderId:  %.*s\n", ORDER_ID_LEN, info->orderId);
          printf("  symbol:   %.*s\n", SYMBOL_LEN, info->symbol);
          printf("  status:   %d\n", info->status);
        }
        break;
      }
      case kCmdSubscribeRsp: {
        printf("  symbols subscribed: %zu\n", bodyLen / sizeof(SubscribeQuoteRequest));
        auto* syms = reinterpret_cast<const SubscribeQuoteRequest*>(body.data());
        for (size_t i = 0; i < bodyLen / sizeof(SubscribeQuoteRequest); ++i) {
          printf("    %.*s.%.*s\n", SYMBOL_LEN, syms[i].symbol,
                 EXCHANGE_LEN, syms[i].exchange);
        }
        break;
      }
      case kCmdTickNotify: {
        if (bodyLen >= sizeof(TickData)) {
          auto* tick = reinterpret_cast<const TickData*>(body.data());
          printf("  symbol:     %.*s\n", SYMBOL_LEN, tick->symbol);
          printf("  exchange:   %.*s\n", EXCHANGE_LEN, tick->exchange);
          printf("  lastPrice:  %.2f\n", tick->lastPrice);
          printf("  bidPrice1:  %.2f  bidVol: %d\n", tick->bidPrice1, tick->bidVolume1);
          printf("  askPrice1:  %.2f  askVol: %d\n", tick->askPrice1, tick->askVolume1);
          printf("  high: %.2f  low: %.2f  open: %.2f\n",
                 tick->highPrice, tick->lowPrice, tick->openPrice);
        }
        break;
      }
      case kCmdOrderNotify: {
        if (bodyLen >= sizeof(OrderInfo)) {
          auto* info = reinterpret_cast<const OrderInfo*>(body.data());
          printf("  orderId:  %.*s  status: %d  volume: %d/%d\n",
                 ORDER_ID_LEN, info->orderId, info->status,
                 info->tradedVolume, info->volume);
        }
        break;
      }
      case kCmdTradeNotify: {
        if (bodyLen >= sizeof(TradeInfo)) {
          auto* trade = reinterpret_cast<const TradeInfo*>(body.data());
          printf("  tradeId: %.*s  orderId: %.*s  symbol: %.*s\n",
                 ORDER_ID_LEN, trade->tradeId, ORDER_ID_LEN, trade->orderId,
                 SYMBOL_LEN, trade->symbol);
          printf("  price: %.2f  volume: %d  direction: %d\n",
                 trade->price, trade->volume, trade->direction);
        }
        break;
      }
      default: {
        // 十六进制 dump 前 64 字节
        printf("  raw[%u]: ", bodyLen);
        size_t dumpLen = bodyLen < 64 ? bodyLen : 64;
        for (size_t i = 0; i < dumpLen; ++i) {
          printf("%02X ", static_cast<unsigned char>(body[i]));
        }
        printf("\n");
        break;
      }
    }
  }

  // 处理心跳请求
  if (header.cmd == kCmdHeartbeatReq) {
    printf("  -> auto-replying HeartbeatRsp\n");
    SendHeartbeatRsp(header.seq);
  }

  // 调用用户回调
  if (responseCb_) {
    responseCb_(header.cmd, header.seq,
                bodyLen > 0 ? body.data() : nullptr, bodyLen);
  }

  return true;
}

// ---- TradeApiDemo ----

int TradeApiDemo::ReqLogin(const LoginRequest* req, int requestId) {
  printf("\n--- Trade ReqLogin (requestId=%d) ---\n", requestId);
  printf("  account: %.*s\n", ACCOUNT_LEN, req->account);
  printf("  password: %.*s\n", PASSWORD_LEN, req->password);
  return client_->SendRequest(kCmdLoginReq, req, sizeof(LoginRequest));
}

int TradeApiDemo::ReqLogout(const LogoutRequest* req, int requestId) {
  printf("\n--- Trade ReqLogout (requestId=%d) ---\n", requestId);
  printf("  account: %.*s\n", ACCOUNT_LEN, req->account);
  return client_->SendRequest(kCmdLogoutReq, req, sizeof(LogoutRequest));
}

int TradeApiDemo::ReqOrderInsert(const OrderRequest* req, int requestId) {
  printf("\n--- Trade ReqOrderInsert (requestId=%d) ---\n", requestId);
  printf("  symbol:   %.*s\n", SYMBOL_LEN, req->symbol);
  printf("  exchange: %.*s\n", EXCHANGE_LEN, req->exchange);
  printf("  price:    %.2f\n", req->price);
  printf("  volume:   %d\n", req->volume);
  printf("  direction: %s\n", req->direction == Direction_Buy ? "Buy" : "Sell");
  printf("  offset:    %s\n",
         req->offset == Offset_Open ? "Open" :
         (req->offset == Offset_Close ? "Close" : "CloseToday"));
  return client_->SendRequest(kCmdOrderInsertReq, req, sizeof(OrderRequest));
}

int TradeApiDemo::ReqOrderCancel(const CancelOrderRequest* req, int requestId) {
  printf("\n--- Trade ReqOrderCancel (requestId=%d) ---\n", requestId);
  printf("  orderId:  %.*s\n", ORDER_ID_LEN, req->orderId);
  printf("  symbol:   %.*s\n", SYMBOL_LEN, req->symbol);
  printf("  exchange: %.*s\n", EXCHANGE_LEN, req->exchange);
  return client_->SendRequest(kCmdOrderCancelReq, req, sizeof(CancelOrderRequest));
}

int TradeApiDemo::ReqQueryAccount(int requestId) {
  printf("\n--- Trade ReqQueryAccount (requestId=%d) ---\n", requestId);
  // 查询类请求无 body，使用空 LoginRequest 作为占位
  LoginRequest placeholder{};
  return client_->SendRequest(kCmdLoginReq, &placeholder, sizeof(placeholder));
}

int TradeApiDemo::ReqQueryPosition(int requestId) {
  printf("\n--- Trade ReqQueryPosition (requestId=%d) [not implemented on server] ---\n", requestId);
  return 0;
}

int TradeApiDemo::ReqQueryOrder(int requestId) {
  printf("\n--- Trade ReqQueryOrder (requestId=%d) [not implemented on server] ---\n", requestId);
  return 0;
}

// ---- QuoteApiDemo ----

int QuoteApiDemo::ReqLogin(const LoginRequest* req, int requestId) {
  printf("\n--- Quote ReqLogin (requestId=%d) ---\n", requestId);
  printf("  account: %.*s\n", ACCOUNT_LEN, req->account);
  return client_->SendRequest(kCmdLoginReq, req, sizeof(LoginRequest));
}

int QuoteApiDemo::ReqLogout(const LogoutRequest* req, int requestId) {
  printf("\n--- Quote ReqLogout (requestId=%d) ---\n", requestId);
  printf("  account: %.*s\n", ACCOUNT_LEN, req->account);
  return client_->SendRequest(kCmdLogoutReq, req, sizeof(LogoutRequest));
}

int QuoteApiDemo::SubscribeQuote(const SubscribeQuoteRequest* req, int count) {
  printf("\n--- Quote SubscribeQuote (count=%d) ---\n", count);
  for (int i = 0; i < count; ++i) {
    printf("  [%d] %.*s.%.*s\n", i,
           SYMBOL_LEN, req[i].symbol, EXCHANGE_LEN, req[i].exchange);
  }
  return client_->SendRequest(kCmdSubscribeReq, req,
                              count * sizeof(SubscribeQuoteRequest));
}

int QuoteApiDemo::UnSubscribeQuote(const UnSubscribeQuoteRequest* req, int count) {
  printf("\n--- Quote UnSubscribeQuote (count=%d) ---\n", count);
  for (int i = 0; i < count; ++i) {
    printf("  [%d] %.*s.%.*s\n", i,
           SYMBOL_LEN, req[i].symbol, EXCHANGE_LEN, req[i].exchange);
  }
  return client_->SendRequest(kCmdUnsubscribeReq, req,
                              count * sizeof(UnSubscribeQuoteRequest));
}

}  // namespace gateway
