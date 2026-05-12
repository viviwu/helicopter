/**
 **************************************************************************
 * @file    :TradeGatewayApi.h
 * @author  :viviwu
 * @brief   :TODO
 * @version :0.1
 * @date    :5/12/26 PM3:59
 * **************************************************************************
 */

#ifndef HELICOPTER_TRADEGATEWAYAPI_H
#define HELICOPTER_TRADEGATEWAYAPI_H

#pragma once

#include "GatewayApi.h"
#include "GatewayStruct.h"

namespace gateway {

class TradeGatewayApi : public GatewayApi {
public:

  virtual int ReqLogin(
      const LoginRequest* req,
      int requestId) = 0;

  virtual int ReqLogout(
      const LogoutRequest* req,
      int requestId) = 0;

  virtual int ReqOrderInsert(
      const OrderRequest* req,
      int requestId) = 0;

  virtual int ReqOrderCancel(
      const CancelOrderRequest* req,
      int requestId) = 0;

  virtual int ReqQueryAccount(
      int requestId) = 0;

  virtual int ReqQueryPosition(
      int requestId) = 0;

  virtual int ReqQueryOrder(
      int requestId) = 0;
};

}

#endif  // HELICOPTER_TRADEGATEWAYAPI_H
