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

#include "gateway/TradeGatewayApi.h"
#include "gateway/QuoteGatewayApi.h"
#include "gateway/GatewaySpi.h"
#include <atomic>

namespace gateway {

class GatewayApiImpl : public TradeGatewayApi, public QuoteGatewayApi {
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

  bool SendToClient(int64_t connId, const void* data, uint32_t len);
  bool Broadcast(const void* data, uint32_t len);

 private:
  GatewaySpi* spi_;
  std::atomic<bool> initialized_;
};

}

#endif  // HELICOPTER_GATEWAYAPIIMPL_H
