/**
 **************************************************************************
 * @file    :GatewayApiImpl.h
 * @author  :viviwu
 * @brief   :Gateway API实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#ifndef HELICOPTER_GATEWAYAPIIMPL_H
#define HELICOPTER_GATEWAYAPIIMPL_H

#include "gateway/TradeApi.h"
#include "gateway/QuoteApi.h"
#include "gateway/GatewaySpi.h"
#include <atomic>
#include <map>
#include <mutex>
#include <cstdint>

namespace gateway {

struct RequestContext {
  int64_t connId;
  uint32_t seq;
  uint16_t cmd;
  uint64_t requestTime;
};

class GatewayApiImpl : public TradeApi, public QuoteApi {
 public:
  GatewayApiImpl();
  ~GatewayApiImpl() override;

  void RegisterSpi(GatewaySpi* spi) override;

  bool Init() override;
  void Join() override;
  void Release() override;

  int ReqLogin(const LoginRequest* req, int requestId) override;
  int ReqLogout(const LogoutRequest* req, int requestId) override;

  int ReqOrderInsert(const OrderRequest* req, int requestId) override;
  int ReqOrderCancel(const CancelOrderRequest* req, int requestId) override;
  int ReqQueryAccount(int requestId) override;
  int ReqQueryPosition(int requestId) override;
  int ReqQueryOrder(int requestId) override;

  int SubscribeQuote(const SubscribeQuoteRequest* req, int count) override;
  int UnSubscribeQuote(const UnSubscribeQuoteRequest* req, int count) override;

  bool SendToClient(int64_t connId, const void* data, uint32_t len) override;
  bool Broadcast(const void* data, uint32_t len) override;

  void SetCurrentConnId(int64_t connId) { currentConnId_ = connId; }

 private:
  uint32_t NextSeq();

  GatewaySpi* spi_;
  std::atomic<bool> initialized_;
  std::atomic<uint32_t> seq_{1};
  std::atomic<int64_t> currentConnId_{0};

  mutable std::mutex mutex_;
  std::map<int, RequestContext> pendingRequests_;
};

}

#endif  // HELICOPTER_GATEWAYAPIIMPL_H
