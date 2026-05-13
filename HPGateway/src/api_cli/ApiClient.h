/**
 **************************************************************************
 * @file    :ApiClient.h
 * @author  :viviwu
 * @brief   :Gateway API客户端（TCP直连 + 协议编解码）
 * @version :0.1
 * @date    :5/12/26
 * **************************************************************************
 */

#ifndef HELICOPTER_APICLIENT_H
#define HELICOPTER_APICLIENT_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <netinet/in.h>

namespace gateway {

struct PacketHeader;
struct LoginRequest;
struct LoginResponse;
struct LogoutRequest;
struct OrderRequest;
struct CancelOrderRequest;
struct OrderInfo;
struct TradeInfo;
struct AccountInfo;
struct PositionInfo;
struct SubscribeQuoteRequest;
struct UnSubscribeQuoteRequest;
struct TickData;

class ApiClient {
 public:
  typedef std::function<void(uint16_t cmd, uint32_t seq, const void* body, size_t bodyLen)>
      ResponseCallback;

  ApiClient();
  ~ApiClient();

  bool Connect(const std::string& host, uint16_t port);
  void Disconnect();
  bool IsConnected() const { return sockfd_ >= 0; }

  // 发送原始报文
  uint32_t SendRequest(uint16_t cmd, const void* data, size_t len);

  // 发送心跳响应
  void SendHeartbeatRsp(uint32_t seq);

  // 接收并解析一个完整的响应包（阻塞，带超时）
  // 返回 true 表示收到包，false 表示超时或连接断开
  bool RecvPacket(int timeoutMs = 5000);

  // 设置响应回调
  void SetResponseCallback(const ResponseCallback& cb) { responseCb_ = cb; }

 private:
  bool RecvAll(void* buf, size_t len);

  int sockfd_;
  uint32_t seq_;
  std::vector<char> recvBuf_;
  ResponseCallback responseCb_;
};

// ---- 便捷包装函数：使用 ApiClient 模拟 TradeGatewayApi / QuoteGatewayApi 接口 ----

struct TradeApiDemo {
  explicit TradeApiDemo(ApiClient* client) : client_(client) {}

  int ReqLogin(const LoginRequest* req, int requestId);
  int ReqLogout(const LogoutRequest* req, int requestId);
  int ReqOrderInsert(const OrderRequest* req, int requestId);
  int ReqOrderCancel(const CancelOrderRequest* req, int requestId);
  int ReqQueryAccount(int requestId);
  int ReqQueryPosition(int requestId);
  int ReqQueryOrder(int requestId);

 private:
  ApiClient* client_;
};

struct QuoteApiDemo {
  explicit QuoteApiDemo(ApiClient* client) : client_(client) {}

  int ReqLogin(const LoginRequest* req, int requestId);
  int ReqLogout(const LogoutRequest* req, int requestId);
  int SubscribeQuote(const SubscribeQuoteRequest* req, int count);
  int UnSubscribeQuote(const UnSubscribeQuoteRequest* req, int count);

 private:
  ApiClient* client_;
};

}  // namespace gateway

#endif  // HELICOPTER_APICLIENT_H
