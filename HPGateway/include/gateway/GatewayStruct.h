/**
 **************************************************************************
 * @file    :GatewayStruct.h
 * @author  :viviwu
 * @brief   :TODO
 * @version :0.1
 * @date    :5/12/26 PM3:31
 * **************************************************************************
 */

#ifndef GATEWAYSTRUCT_H
#define GATEWAYSTRUCT_H


#pragma once

#include <stdint.h>

namespace gateway {

static const int ACCOUNT_LEN = 32;
static const int PASSWORD_LEN = 64;
static const int SYMBOL_LEN = 32;
static const int EXCHANGE_LEN = 16;
static const int ORDER_ID_LEN = 64;
static const int ERROR_MSG_LEN = 256;

enum Direction {
    Direction_Buy = 0,
    Direction_Sell = 1
};

enum OffsetFlag {
    Offset_Open = 0,
    Offset_Close = 1,
    Offset_CloseToday = 2
};

enum OrderStatus {
    OrderStatus_Unknown = 0,

    OrderStatus_Submitted,
    OrderStatus_Accepted,

    OrderStatus_PartFilled,
    OrderStatus_Filled,

    OrderStatus_Canceled,
    OrderStatus_Rejected
};

enum ErrorCode {
    ErrorCode_Success = 0,

    ErrorCode_Network = 1001,
    ErrorCode_Timeout = 1002,

    ErrorCode_LoginFailed = 2001,
    ErrorCode_NotLogin = 2002,

    ErrorCode_OrderRejected = 3001
};

struct ErrorInfo {
    int errorCode;
    char errorMsg[ERROR_MSG_LEN];
};

struct LoginRequest {
    char account[ACCOUNT_LEN];
    char password[PASSWORD_LEN];
};

struct LoginResponse {
    int tradingDay;

    char account[ACCOUNT_LEN];

    double balance;
    double available;

    int frontId;
    int sessionId;
};

struct LogoutRequest {
    char account[ACCOUNT_LEN];
};

struct Instrument {
    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];

    double tickSize;
    int volumeMultiple;
};

struct OrderRequest {

    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];

    char clientOrderId[ORDER_ID_LEN];

    Direction direction;
    OffsetFlag offset;

    double price;
    int volume;
};

struct CancelOrderRequest {

    char orderId[ORDER_ID_LEN];

    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];
};

struct OrderInfo {

    char orderId[ORDER_ID_LEN];

    char clientOrderId[ORDER_ID_LEN];

    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];

    Direction direction;
    OffsetFlag offset;

    double price;

    int volume;
    int tradedVolume;

    OrderStatus status;

    uint64_t submitTime;
    uint64_t updateTime;
};

struct TradeInfo {

    char tradeId[ORDER_ID_LEN];

    char orderId[ORDER_ID_LEN];

    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];

    Direction direction;
    OffsetFlag offset;

    double price;

    int volume;

    uint64_t tradeTime;
};

struct PositionInfo {

    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];

    Direction direction;

    int volume;

    int ydVolume;

    double avgPrice;

    double pnl;
};

struct AccountInfo {

    char account[ACCOUNT_LEN];

    double balance;

    double available;

    double frozen;

    double margin;

    double pnl;
};

struct SubscribeQuoteRequest {
    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];
};

struct UnSubscribeQuoteRequest {
    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];
};

struct TickData {

    char symbol[SYMBOL_LEN];
    char exchange[EXCHANGE_LEN];

    double lastPrice;

    double bidPrice1;
    int bidVolume1;

    double askPrice1;
    int askVolume1;

    double highPrice;
    double lowPrice;
    double openPrice;

    uint64_t updateTime;
};

}


#endif  // GATEWAYSTRUCT_H
