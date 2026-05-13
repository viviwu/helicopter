/**
 **************************************************************************
 * @file    :QuoteApi.h
 * @author  :viviwu
 * @brief   :TODO
 * @version :0.1
 * @date    :5/12/26 PM3:59
 * **************************************************************************
 */

#ifndef HELICOPTER_QUOTEGATEWAYAPI_H
#define HELICOPTER_QUOTEGATEWAYAPI_H

#pragma once

#include "GatewayApi.h"
#include "GatewayStruct.h"

namespace gateway {

class QuoteApi : public GatewayApi {
public:

  virtual int ReqLogin(
      const LoginRequest* req,
      int requestId) = 0;

  virtual int ReqLogout(
      const LogoutRequest* req,
      int requestId) = 0;

  virtual int SubscribeQuote(
      const SubscribeQuoteRequest* req,
      int count) = 0;

  virtual int UnSubscribeQuote(
      const UnSubscribeQuoteRequest* req,
      int count) = 0;
};

}

#endif  // HELICOPTER_QUOTEGATEWAYAPI_H
