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
#include "../base/Logger.h"
#include <cassert>

namespace gateway {

GatewayApiImpl::GatewayApiImpl()
    : spi_(nullptr),
      initialized_(false) {}

GatewayApiImpl::~GatewayApiImpl() {
  Release();
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
  LOG_INFO("ReqLogin account={}", req->account);
  return 0;
}

int GatewayApiImpl::ReqLogout(const LogoutRequest* req, int requestId) {
  LOG_INFO("ReqLogout account={}", req->account);
  return 0;
}

int GatewayApiImpl::ReqOrderInsert(const OrderRequest* req, int requestId) {
  LOG_INFO("ReqOrderInsert symbol={} price={} volume={}", req->symbol, req->price, req->volume);
  return 0;
}

int GatewayApiImpl::ReqOrderCancel(const CancelOrderRequest* req, int requestId) {
  LOG_INFO("ReqOrderCancel orderId={}", req->orderId);
  return 0;
}

int GatewayApiImpl::ReqQueryAccount(int requestId) {
  LOG_INFO("ReqQueryAccount");
  return 0;
}

int GatewayApiImpl::ReqQueryPosition(int requestId) {
  LOG_INFO("ReqQueryPosition");
  return 0;
}

int GatewayApiImpl::ReqQueryOrder(int requestId) {
  LOG_INFO("ReqQueryOrder");
  return 0;
}

int GatewayApiImpl::SubscribeQuote(const SubscribeQuoteRequest* req, int count) {
  LOG_INFO("SubscribeQuote symbol={}", req->symbol);
  return 0;
}

int GatewayApiImpl::UnSubscribeQuote(const UnSubscribeQuoteRequest* req, int count) {
  LOG_INFO("UnSubscribeQuote symbol={}", req->symbol);
  return 0;
}

bool GatewayApiImpl::SendToClient(int64_t connId, const void* data, uint32_t len) {
  return ConnectionManager::Instance().SendToClient(connId, data, len);
}

bool GatewayApiImpl::Broadcast(const void* data, uint32_t len) {
  return ConnectionManager::Instance().Broadcast(data, len);
}

}
