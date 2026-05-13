/**
 **************************************************************************
 * @file    :main.cpp (api_cli)
 * @author  :viviwu
 * @brief   :Gateway API 客户端测试工具
 * @version :0.1
 * @date    :5/12/26
 * **************************************************************************
 *
 * 用法:
 *   ./api_cli                          # 交互模式，连接 trade:8001 + quote:8002
 *   ./api_cli --demo                   # 自动运行演示场景
 *   ./api_cli --trade-only             # 仅连接 Trade 通道
 *   ./api_cli --quote-only             # 仅连接 Quote 通道
 *   ./api_cli --host 192.168.1.100     # 指定服务器地址
 *   ./api_cli --trade-port 9001        # 指定 Trade 端口
 *   ./api_cli --quote-port 9002        # 指定 Quote 端口
 */

#include "ApiClient.h"
#include "../protocol/Packet.h"
#include "gateway/GatewayStruct.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <poll.h>

using namespace gateway;

static volatile bool g_running = true;

static void SignalHandler(int) {
  g_running = false;
}

// ---- 演示场景：Trade API ----
static void DemoTradeApi(ApiClient& client) {
  printf("\n");
  printf("========================================\n");
  printf("  TradeApi Demo\n");
  printf("========================================\n");

  TradeApiDemo trade(&client);

  // 1. Login
  LoginRequest loginReq{};
  strncpy(loginReq.account, "trader001", ACCOUNT_LEN - 1);
  strncpy(loginReq.password, "pass123", PASSWORD_LEN - 1);
  trade.ReqLogin(&loginReq, 1);
  client.RecvPacket(3000);

  // 2. OrderInsert (Buy Open)
  OrderRequest orderReq{};
  strncpy(orderReq.symbol, "rb2512", SYMBOL_LEN - 1);
  strncpy(orderReq.exchange, "SHFE", EXCHANGE_LEN - 1);
  strncpy(orderReq.clientOrderId, "cli-ord-001", ORDER_ID_LEN - 1);
  orderReq.direction = Direction_Buy;
  orderReq.offset = Offset_Open;
  orderReq.price = 3250.0;
  orderReq.volume = 10;
  trade.ReqOrderInsert(&orderReq, 2);
  client.RecvPacket(3000);

  // 3. OrderInsert (Sell Close)
  strncpy(orderReq.clientOrderId, "cli-ord-002", ORDER_ID_LEN - 1);
  orderReq.direction = Direction_Sell;
  orderReq.offset = Offset_Close;
  orderReq.price = 3280.0;
  orderReq.volume = 5;
  trade.ReqOrderInsert(&orderReq, 3);
  client.RecvPacket(3000);

  // 4. OrderCancel
  CancelOrderRequest cancelReq{};
  strncpy(cancelReq.orderId, "cli-ord-001", ORDER_ID_LEN - 1);
  strncpy(cancelReq.symbol, "rb2512", SYMBOL_LEN - 1);
  strncpy(cancelReq.exchange, "SHFE", EXCHANGE_LEN - 1);
  trade.ReqOrderCancel(&cancelReq, 4);
  client.RecvPacket(3000);

  // 5. QueryAccount (server not implemented yet)
  trade.ReqQueryAccount(5);
  client.RecvPacket(2000);

  // 6. Logout
  LogoutRequest logoutReq{};
  strncpy(logoutReq.account, "trader001", ACCOUNT_LEN - 1);
  trade.ReqLogout(&logoutReq, 6);
  client.RecvPacket(3000);

  printf("\n=== Trade API Demo Complete ===\n");
}

// ---- 演示场景：Quote API ----
static void DemoQuoteApi(ApiClient& client) {
  printf("\n");
  printf("========================================\n");
  printf("  QuoteApi Demo\n");
  printf("========================================\n");

  QuoteApiDemo quote(&client);

  // 1. Login
  LoginRequest loginReq{};
  strncpy(loginReq.account, "quoteguest", ACCOUNT_LEN - 1);
  strncpy(loginReq.password, "guest123", PASSWORD_LEN - 1);
  quote.ReqLogin(&loginReq, 1);
  client.RecvPacket(3000);

  // 2. SubscribeQuote (3 symbols)
  SubscribeQuoteRequest subReqs[3];
  memset(&subReqs, 0, sizeof(subReqs));
  strncpy(subReqs[0].symbol, "rb2512", SYMBOL_LEN - 1);
  strncpy(subReqs[0].exchange, "SHFE", EXCHANGE_LEN - 1);
  strncpy(subReqs[1].symbol, "hc2512", SYMBOL_LEN - 1);
  strncpy(subReqs[1].exchange, "SHFE", EXCHANGE_LEN - 1);
  strncpy(subReqs[2].symbol, "IF2512", SYMBOL_LEN - 1);
  strncpy(subReqs[2].exchange, "CFFEX", EXCHANGE_LEN - 1);
  quote.SubscribeQuote(subReqs, 3);
  client.RecvPacket(3000);

  // 3. UnSubscribeQuote (1 symbol)
  UnSubscribeQuoteRequest unsubReq;
  memset(&unsubReq, 0, sizeof(unsubReq));
  strncpy(unsubReq.symbol, "IF2512", SYMBOL_LEN - 1);
  strncpy(unsubReq.exchange, "CFFEX", EXCHANGE_LEN - 1);
  quote.UnSubscribeQuote(&unsubReq, 1);
  client.RecvPacket(3000);

  // 4. Logout
  LogoutRequest logoutReq{};
  strncpy(logoutReq.account, "quoteguest", ACCOUNT_LEN - 1);
  quote.ReqLogout(&logoutReq, 2);
  client.RecvPacket(3000);

  printf("\n=== Quote API Demo Complete ===\n");
}

// ---- 交互菜单 ----
static void PrintMenu() {
  printf("\n");
  printf("╔══════════════════════════════════════════╗\n");
  printf("║       Gateway API Client Tester          ║\n");
  printf("╠══════════════════════════════════════════╣\n");
  printf("║  TRADE API:                              ║\n");
  printf("║   1. Login                               ║\n");
  printf("║   2. Logout                              ║\n");
  printf("║   3. OrderInsert (Buy)                   ║\n");
  printf("║   4. OrderInsert (Sell)                  ║\n");
  printf("║   5. OrderCancel                         ║\n");
  printf("╠══════════════════════════════════════════╣\n");
  printf("║  QUOTE API:                              ║\n");
  printf("║   6. SubscribeQuote (rb2512)             ║\n");
  printf("║   7. UnSubscribeQuote (rb2512)           ║\n");
  printf("╠══════════════════════════════════════════╣\n");
  printf("║  GENERAL:                                ║\n");
  printf("║   d. Run Trade API Demo                  ║\n");
  printf("║   q. Run Quote API Demo                  ║\n");
  printf("║   a. Run All Demos                       ║\n");
  printf("║   h. Print this help                     ║\n");
  printf("║   0. Exit                                ║\n");
  printf("╚══════════════════════════════════════════╝\n");
  printf("Choice> ");
}

static void InteractiveLoop(ApiClient& tradeClient, ApiClient& quoteClient) {
  TradeApiDemo tradeTrade(&tradeClient);
  QuoteApiDemo tradeQuote(&quoteClient);

  PrintMenu();

  char buf[256];
  while (g_running) {
    if (fgets(buf, sizeof(buf), stdin) == nullptr) break;

    char choice = buf[0];
    switch (choice) {
      case '1': {
        LoginRequest req{};
        strncpy(req.account, "trader001", ACCOUNT_LEN - 1);
        strncpy(req.password, "pass123", PASSWORD_LEN - 1);
        tradeTrade.ReqLogin(&req, 100);
        tradeClient.RecvPacket(3000);
        break;
      }
      case '2': {
        LogoutRequest req{};
        strncpy(req.account, "trader001", ACCOUNT_LEN - 1);
        tradeTrade.ReqLogout(&req, 101);
        tradeClient.RecvPacket(3000);
        break;
      }
      case '3': {
        OrderRequest req{};
        strncpy(req.symbol, "rb2512", SYMBOL_LEN - 1);
        strncpy(req.exchange, "SHFE", EXCHANGE_LEN - 1);
        snprintf(req.clientOrderId, ORDER_ID_LEN, "cli-%ld", time(nullptr));
        req.direction = Direction_Buy;
        req.offset = Offset_Open;
        req.price = 3250.0;
        req.volume = 10;
        tradeTrade.ReqOrderInsert(&req, 102);
        tradeClient.RecvPacket(3000);
        break;
      }
      case '4': {
        OrderRequest req{};
        strncpy(req.symbol, "rb2512", SYMBOL_LEN - 1);
        strncpy(req.exchange, "SHFE", EXCHANGE_LEN - 1);
        snprintf(req.clientOrderId, ORDER_ID_LEN, "cli-%ld", time(nullptr));
        req.direction = Direction_Sell;
        req.offset = Offset_Close;
        req.price = 3280.0;
        req.volume = 5;
        tradeTrade.ReqOrderInsert(&req, 103);
        tradeClient.RecvPacket(3000);
        break;
      }
      case '5': {
        CancelOrderRequest req{};
        strncpy(req.orderId, "cli-ord-001", ORDER_ID_LEN - 1);
        strncpy(req.symbol, "rb2512", SYMBOL_LEN - 1);
        strncpy(req.exchange, "SHFE", EXCHANGE_LEN - 1);
        tradeTrade.ReqOrderCancel(&req, 104);
        tradeClient.RecvPacket(3000);
        break;
      }
      case '6': {
        SubscribeQuoteRequest req;
        memset(&req, 0, sizeof(req));
        strncpy(req.symbol, "rb2512", SYMBOL_LEN - 1);
        strncpy(req.exchange, "SHFE", EXCHANGE_LEN - 1);
        tradeQuote.SubscribeQuote(&req, 1);
        tradeClient.RecvPacket(3000);
        break;
      }
      case '7': {
        UnSubscribeQuoteRequest req;
        memset(&req, 0, sizeof(req));
        strncpy(req.symbol, "rb2512", SYMBOL_LEN - 1);
        strncpy(req.exchange, "SHFE", EXCHANGE_LEN - 1);
        tradeQuote.UnSubscribeQuote(&req, 1);
        tradeClient.RecvPacket(3000);
        break;
      }
      case 'd':
        DemoTradeApi(tradeClient);
        break;
      case 'q':
        DemoQuoteApi(tradeClient);  // 当前服务器仅启动了 Trade 端口，用 trade 通道演示
        break;
      case 'a':
        DemoTradeApi(tradeClient);
        DemoQuoteApi(tradeClient);
        break;
      case 'h':
        PrintMenu();
        break;
      case '0':
        printf("Exiting...\n");
        g_running = false;
        return;
      default:
        if (choice != '\n') {
          printf("Unknown choice '%c'. Press 'h' for help.\n", choice);
        }
        break;
    }

    if (g_running && choice != 'h' && choice != '\n') {
      printf("\nChoice> ");
      fflush(stdout);
    }
  }
}

// ---- 入口 ----
int main(int argc, char* argv[]) {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGPIPE, SIG_IGN);

  // 解析命令行参数
  bool demoMode = false;
  bool tradeOnly = false;
  bool quoteOnly = false;
  const char* host = "127.0.0.1";
  uint16_t tradePort = 8001;
  uint16_t quotePort = 8002;

  static struct option longOpts[] = {
    {"demo",        no_argument,       nullptr, 'D'},
    {"trade-only",  no_argument,       nullptr, 'T'},
    {"quote-only",  no_argument,       nullptr, 'Q'},
    {"host",        required_argument, nullptr, 'h'},
    {"trade-port",  required_argument, nullptr, 'p'},
    {"quote-port",  required_argument, nullptr, 'q'},
    {"help",        no_argument,       nullptr, 'H'},
    {nullptr, 0, nullptr, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "DTQh:p:q:H", longOpts, nullptr)) != -1) {
    switch (opt) {
      case 'D': demoMode = true; break;
      case 'T': tradeOnly = true; break;
      case 'Q': quoteOnly = true; break;
      case 'h': host = optarg; break;
      case 'p': tradePort = static_cast<uint16_t>(atoi(optarg)); break;
      case 'q': quotePort = static_cast<uint16_t>(atoi(optarg)); break;
      case 'H':
        printf("Usage: %s [OPTIONS]\n"
               "  --demo          Run demo scenarios automatically\n"
               "  --trade-only    Only connect to Trade channel\n"
               "  --quote-only    Only connect to Quote channel\n"
               "  --host HOST     Server host (default: 127.0.0.1)\n"
               "  --trade-port P  Trade port (default: 8001)\n"
               "  --quote-port P  Quote port (default: 8002)\n"
               "  --help          This help\n",
               argv[0]);
        return 0;
      default:
        return 1;
    }
  }

  printf("Gateway API Client Tester\n");
  printf("Server: %s  Trade=%u  Quote=%u\n\n", host, tradePort, quotePort);

  // 连接 Trade 通道
  ApiClient tradeClient;
  bool tradeOk = false;
  if (!quoteOnly) {
    tradeOk = tradeClient.Connect(host, tradePort);
    if (!tradeOk) {
      fprintf(stderr, "WARNING: Failed to connect to Trade channel at %s:%u\n", host, tradePort);
    }
  }

  // 连接 Quote 通道（服务器当前可能未开放 8002 端口）
  ApiClient quoteClient;
  bool quoteOk = false;
  if (!tradeOnly) {
    quoteOk = quoteClient.Connect(host, quotePort);
    if (!quoteOk) {
      printf("NOTE: Quote channel at %s:%u not available (server may not have it enabled yet)\n",
             host, quotePort);
    }
  }

  if (!tradeOk && !quoteOk) {
    fprintf(stderr, "ERROR: No channels available. Is the gateway server running?\n");
    return 1;
  }

  if (demoMode) {
    if (tradeOk) {
      DemoTradeApi(tradeClient);
      if (!quoteOk) DemoQuoteApi(tradeClient);  // 使用 trade 通道演示 quote API
    }
    if (quoteOk) {
      DemoQuoteApi(quoteClient);
    }
  } else {
    InteractiveLoop(tradeOk ? tradeClient : quoteClient,
                    quoteOk ? quoteClient : tradeClient);
  }

  tradeClient.Disconnect();
  quoteClient.Disconnect();

  printf("\nDone.\n");
  return 0;
}
