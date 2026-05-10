/**
  ******************************************************************************
  * @file           : server.cpp
  * @author         : vivi wu
  * @brief          : Gateway 服务端入口（TradeGateway :12345 + QuoteGateway :12346）
  * @version        : 1.1.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include <iostream>
#include <csignal>
#include <string>

#include "trade/TradeGatewayServer.h"
#include "trade/TradeHandler.h"
#include "quote/QuoteGatewayServer.h"
#include "common/ThreadPool.h"

#include "trade.pb.h"

using namespace trade;
using namespace quote;

// ============================================================================
// 交易业务处理器
// ============================================================================
class MyTradeHandler : public TradeHandler {
public:
    bool OnLogin(const trade_proto::LoginRequest& req,
                 trade_proto::LoginResponse& rsp) override {
        rsp.set_request_id(req.request_id());

        if (req.username() == "admin" && req.password() == "123456") {
            auto* account = rsp.mutable_account();
            account->set_user_id(1001);
            account->set_username("admin");
            account->set_email("admin@example.com");
            account->set_role("admin");
            rsp.set_error_code(0);
            rsp.set_error_msg("Login successful");
        } else {
            rsp.set_error_code(1);
            rsp.set_error_msg("Invalid username or password");
        }
        return true;
    }

    bool OnPlaceOrder(const trade_proto::PlaceOrderRequest& req,
                      trade_proto::PlaceOrderResponse& rsp) override {
        rsp.set_request_id(req.request_id());
        rsp.set_order_id(++nextOrderId_);
        rsp.set_error_code(0);
        rsp.set_error_msg("Order placed");
        std::cout << "[TradeHandler] Order: " << req.symbol()
                  << " " << (req.side() == 1 ? "BUY" : "SELL")
                  << " " << req.quantity() << "@" << req.price()
                  << " → order_id=" << rsp.order_id() << "\n";
        return true;
    }

    bool OnQueryOrder(const trade_proto::QueryOrderRequest& req,
                      trade_proto::QueryOrderResponse& rsp) override {
        rsp.set_request_id(req.request_id());
        rsp.set_order_id(req.order_id());
        rsp.set_symbol("BTC-USDT");
        rsp.set_status(0);
        rsp.set_error_code(0);
        rsp.set_error_msg("OK");
        return true;
    }

    bool OnCancelOrder(const trade_proto::CancelOrderRequest& req,
                       trade_proto::CancelOrderResponse& rsp) override {
        rsp.set_request_id(req.request_id());
        rsp.set_error_code(0);
        rsp.set_error_msg("Order cancelled");
        return true;
    }

private:
    uint64_t nextOrderId_ = 1000;
};

// ============================================================================
// 全局
// ============================================================================
namespace {
    TradeGatewayServer* g_tradeGw = nullptr;
    QuoteGatewayServer* g_quoteGw = nullptr;
    ThreadPool*  g_pool    = nullptr;
    volatile std::sig_atomic_t g_shutdownRequested = 0;
}

static void signalHandler(int /*sig*/) {
    g_shutdownRequested = 1;
}

static void printMenu() {
    std::cout << "\n========== Gateway Server Menu ==========\n"
              << "  Trade :" << g_tradeGw->GetActiveConnections()
              << " clients on :12345\n"
              << "  Quote : PUB on :12346\n"
              << "------------------------------------------\n"
              << "1. Publish notice to all subscribers\n"
              << "2. Publish simulated market data\n"
              << "3. Shutdown server\n"
              << "=========================================\n"
              << "Please select: ";
}

// ============================================================================
// main
// ============================================================================
int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    ThreadPool pool(4);

    // ---- TradeGateway (ROUTER on :12345) ----
    TradeGatewayServer tradeGw;
    MyTradeHandler tradeHandler;
    tradeGw.RegisterHandler(&tradeHandler);
    tradeGw.SetThreadPool(&pool);
    tradeGw.SetMaxConnections(10000);
    tradeGw.SetIdleTimeout(60);

    if (!tradeGw.Start("0.0.0.0", 12345)) {
        std::cerr << "Failed to start TradeGateway\n";
        return 1;
    }

    // ---- QuoteGateway (PUB on :12346) ----
    QuoteGatewayServer quoteGw;
    if (!quoteGw.Start("0.0.0.0", 12346)) {
        std::cerr << "Failed to start QuoteGateway\n";
        tradeGw.Stop();
        return 1;
    }

    g_tradeGw = &tradeGw;
    g_quoteGw = &quoteGw;
    g_pool    = &pool;

    std::cout << "\nGateway server running (" << pool.WorkerCount()
              << " workers).\n";

    while (!g_shutdownRequested) {
        printMenu();
        std::string choice;
        if (!std::getline(std::cin, choice)) break;

        if (choice == "1") {
            std::cout << "Enter notice content: ";
            std::string msg;
            if (!std::getline(std::cin, msg)) break;
            quoteGw.PublishNotice(msg);
        }
        else if (choice == "2") {
            std::cout << "Symbol (e.g. BTC-USDT): ";
            std::string symbol;
            if (!std::getline(std::cin, symbol) || symbol.empty()) {
                symbol = "BTC-USDT";
            }
            double price = 50000.0 + (rand() % 10000) / 100.0;
            double volume = 1.0 + (rand() % 100) / 10.0;
            quoteGw.PublishMarketData(symbol, price, volume);
        }
        else if (choice == "3") {
            std::cout << "Shutting down...\n";
            g_shutdownRequested = 1;
        }
        else {
            std::cout << "Invalid choice.\n";
        }
    }

    std::cout << "\nShutting down...\n";

    quoteGw.Stop();
    tradeGw.Stop();
    pool.Shutdown();

    std::cout << "Gateway server stopped.\n";
    return 0;
}
