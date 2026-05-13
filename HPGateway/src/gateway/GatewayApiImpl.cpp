/**
 **************************************************************************
 * @file    :GatewayApiImpl.cpp
 * @author  :viviwu
 * @brief   :Gateway API实现
 * @version :0.1
 * @date    :5/12/26 PM3:42
 * **************************************************************************
 */

#include "GatewayApiImpl.h"
#include "ConnectionManager.h"
#include "SessionManager.h"
#include "../protocol/Codec.h"
#include "../base/Logger.h"
#include "../base/Timestamp.h"
#include <cassert>
#include <cstring>

namespace gateway {

GatewayApiImpl::GatewayApiImpl()
    : spi_(nullptr),
      initialized_(false) {}

GatewayApiImpl::~GatewayApiImpl() {
  Release();
}

uint32_t GatewayApiImpl::NextSeq() {
  return seq_.fetch_add(1);
}

void GatewayApiImpl::RegisterSpi(GatewaySpi* spi) {
  spi_ = spi;
}

bool GatewayApiImpl::Init() {
  initialized_ = true;
  LOG_INFO("GatewayApiImpl::Init OK");
  if (spi_) {
    spi_->OnConnected();
  }
  return true;
}

void GatewayApiImpl::Join() {
}

void GatewayApiImpl::Release() {
  initialized_ = false;
  if (spi_) {
    spi_->OnDisconnected(0);
  }
}

int GatewayApiImpl::ReqLogin(const LoginRequest* req, int requestId) {
  LOG_INFO("ReqLogin account={} requestId={}", req->account, requestId);
  uint32_t seq = NextSeq();

  Packet packet = Codec::Encode(kCmdLoginReq, seq, req, sizeof(LoginRequest));

  RequestContext ctx;
  ctx.connId = currentConnId_.load();
  ctx.seq = seq;
  ctx.cmd = kCmdLoginReq;
  ctx.requestTime = Timestamp::Now().MilliSecondsSinceEpoch();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingRequests_[requestId] = ctx;
  }

  // Phase 1: packet would be sent to backend via IO thread
  // For now, send back to client as echo if connected
  if (ctx.connId > 0) {
    SendToClient(ctx.connId, &packet, packet.TotalLength());
  }
  return 0;
}

int GatewayApiImpl::ReqLogout(const LogoutRequest* req, int requestId) {
  LOG_INFO("ReqLogout account={} requestId={}", req->account, requestId);
  uint32_t seq = NextSeq();
  Packet packet = Codec::Encode(kCmdLogoutReq, seq, req, sizeof(LogoutRequest));

  int64_t connId = currentConnId_.load();
  if (connId > 0) {
    SendToClient(connId, &packet, packet.TotalLength());
  }
  return 0;
}

int GatewayApiImpl::ReqOrderInsert(const OrderRequest* req, int requestId) {
  LOG_INFO("ReqOrderInsert symbol={} price={} volume={} requestId={}",
           req->symbol, req->price, req->volume, requestId);
  uint32_t seq = NextSeq();

  Packet packet = Codec::Encode(kCmdOrderInsertReq, seq, req, sizeof(OrderRequest));

  RequestContext ctx;
  ctx.connId = currentConnId_.load();
  ctx.seq = seq;
  ctx.cmd = kCmdOrderInsertReq;
  ctx.requestTime = Timestamp::Now().MilliSecondsSinceEpoch();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingRequests_[requestId] = ctx;
  }
  return 0;
}

int GatewayApiImpl::ReqOrderCancel(const CancelOrderRequest* req, int requestId) {
  LOG_INFO("ReqOrderCancel orderId={} requestId={}", req->orderId, requestId);
  uint32_t seq = NextSeq();

  Packet packet = Codec::Encode(kCmdOrderCancelReq, seq, req, sizeof(CancelOrderRequest));

  RequestContext ctx;
  ctx.connId = currentConnId_.load();
  ctx.seq = seq;
  ctx.cmd = kCmdOrderCancelReq;
  ctx.requestTime = Timestamp::Now().MilliSecondsSinceEpoch();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingRequests_[requestId] = ctx;
  }
  return 0;
}

int GatewayApiImpl::ReqQueryAccount(int requestId) {
  LOG_INFO("ReqQueryAccount requestId={}", requestId);
  uint32_t seq = NextSeq();
  RequestContext ctx;
  ctx.connId = currentConnId_.load();
  ctx.seq = seq;
  ctx.cmd = 0;
  ctx.requestTime = Timestamp::Now().MilliSecondsSinceEpoch();
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingRequests_[requestId] = ctx;
  }
  return 0;
}

int GatewayApiImpl::ReqQueryPosition(int requestId) {
  LOG_INFO("ReqQueryPosition requestId={}", requestId);
  return 0;
}

int GatewayApiImpl::ReqQueryOrder(int requestId) {
  LOG_INFO("ReqQueryOrder requestId={}", requestId);
  return 0;
}

int GatewayApiImpl::SubscribeQuote(const SubscribeQuoteRequest* req, int count) {
  LOG_INFO("SubscribeQuote symbol={} count={}", req->symbol, count);
  for (int i = 0; i < count; ++i) {
    LOG_DEBUG("  subscribe {}.{}", req[i].symbol, req[i].exchange);
  }
  return 0;
}

int GatewayApiImpl::UnSubscribeQuote(const UnSubscribeQuoteRequest* req, int count) {
  LOG_INFO("UnSubscribeQuote symbol={} count={}", req->symbol, count);
  return 0;
}

bool GatewayApiImpl::SendToClient(int64_t connId, const void* data, uint32_t len) {
  return ConnectionManager::Instance().SendToClient(connId, data, len);
}

bool GatewayApiImpl::Broadcast(const void* data, uint32_t len) {
  return ConnectionManager::Instance().Broadcast(data, len);
}

}
