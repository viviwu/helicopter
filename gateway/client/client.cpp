/**
  ******************************************************************************
  * @file           : client.cpp
  * @author         : vivi wu
  * @brief          : 客户端 Demo（TradeApi + QuoteApi 双通道）
  * @version        : 1.0.0
  * @date           : 10/05/26
  ******************************************************************************
  */
#include <iostream>
#include <string>
#include <atomic>
#include <csignal>

#include "trade/TradeApi.h"
#include "quote/QuoteApi.h"

using namespace trade;
using namespace quote;

// ============================================================================
// 交易回调
// ============================================================================
class MyTradeSpi : public TradeSpi {
public:
    void OnFrontConnected() override {
        std::cout << "\n[Trade] Connected" << std::endl;
        connected_ = true;
    }

    void OnFrontDisconnected(int reason) override {
        std::cout << "\n[Trade] Disconnected (reason=" << reason << ")" << std::endl;
        connected_ = false;
    }

    void OnLogin(const LoginResponse& rsp) override {
        std::cout << "\n[Trade] Login response:" << std::endl;
        if (rsp.error_code == 0) {
            std::cout << "  OK - user_id=" << rsp.account.user_id
                      << " username=" << rsp.account.username << std::endl;
        } else {
            std::cout << "  FAILED - " << rsp.error_msg << std::endl;
        }
    }

    void OnPlaceOrder(const PlaceOrderResponse& rsp) override {
        std::cout << "\n[Trade] PlaceOrder response:" << std::endl;
        if (rsp.error_code == 0) {
            std::cout << "  OK - order_id=" << rsp.order_id << std::endl;
        } else {
            std::cout << "  FAILED - " << rsp.error_msg << std::endl;
        }
    }

    void OnQueryOrder(const QueryOrderResponse& rsp) override {
        std::cout << "\n[Trade] QueryOrder response:" << std::endl;
        std::cout << "  order_id=" << rsp.order_id
                  << " symbol=" << rsp.symbol
                  << " status=" << rsp.status
                  << " filled=" << rsp.filled_qty << std::endl;
    }

    std::atomic<bool> connected_{false};
};

// ============================================================================
// 行情回调
// ============================================================================
class MyQuoteSpi : public QuoteSpi {
public:
    void OnFrontConnected() override {
        std::cout << "\n[Quote] Connected" << std::endl;
        connected_ = true;
    }

    void OnFrontDisconnected(int reason) override {
        std::cout << "\n[Quote] Disconnected (reason=" << reason << ")" << std::endl;
        connected_ = false;
    }

    void OnMarketData(const MarketData& data) override {
        std::cout << "\n[Quote] >>> MARKET DATA <<<" << std::endl;
        std::cout << "  symbol : " << data.symbol << std::endl;
        std::cout << "  price  : " << data.price << std::endl;
        std::cout << "  volume : " << data.volume << std::endl;
    }

    void OnNotice(const Notice& notice) override {
        std::cout << "\n[Quote] >>> NOTICE <<<" << std::endl;
        std::cout << "  content : " << notice.content << std::endl;
        std::cout << "  msg_id  : " << notice.message_id << std::endl;
    }

    std::atomic<bool> connected_{false};
};

// ============================================================================
// 菜单
// ============================================================================
static void printMenu(bool tradeConnected, bool quoteConnected) {
    std::cout << "\n========== Gateway Client Demo ==========" << std::endl;
    std::cout << "Trade: " << (tradeConnected ? "Connected" : "Disconnected")
              << " | Quote: " << (quoteConnected ? "Connected" : "Disconnected")
              << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "1. Connect TradeGateway + Login" << std::endl;
    std::cout << "2. Place order (demo)" << std::endl;
    std::cout << "3. Query order (demo)" << std::endl;
    std::cout << "4. Connect QuoteGateway + Subscribe" << std::endl;
    std::cout << "5. Listen for quote data (Enter to stop)" << std::endl;
    std::cout << "6. Disconnect all" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Please select: ";
}

// ============================================================================
// main
// ============================================================================
int main() {
    TradeApi tradeApi;
    QuoteApi quoteApi;

    MyTradeSpi tradeSpi;
    MyQuoteSpi quoteSpi;

    tradeApi.RegisterSpi(&tradeSpi);
    quoteApi.RegisterSpi(&quoteSpi);

    bool running = true;
    uint64_t nextOrderId = 1;

    while (running) {
        printMenu(tradeSpi.connected_.load(), quoteSpi.connected_.load());

        std::string choice;
        if (!std::getline(std::cin, choice)) break;

        if (choice == "1") {
            // Connect TradeGateway
            if (tradeApi.IsConnected()) {
                std::cout << "Trade already connected." << std::endl;
                continue;
            }
            int ret = tradeApi.Connect("127.0.0.1", 12345, 5);
            if (ret != 0) {
                std::cerr << "Failed to connect TradeGateway." << std::endl;
                continue;
            }
            // Login
            LoginRequest req;
            req.request_id = 1;
            req.username = "admin";
            req.password = "123456";
            tradeApi.Login(req);
            std::cout << "Trade connected & login sent." << std::endl;
        }
        else if (choice == "2") {
            if (!tradeSpi.connected_) {
                std::cerr << "Trade not connected." << std::endl;
                continue;
            }
            PlaceOrderRequest req;
            req.request_id = nextOrderId++;
            req.symbol = "BTC-USDT";
            req.side = 1;       // buy
            req.order_type = 1; // limit
            req.price = 50000.0;
            req.quantity = 0.1;
            tradeApi.PlaceOrder(req);
            std::cout << "PlaceOrder sent." << std::endl;
        }
        else if (choice == "3") {
            if (!tradeSpi.connected_) {
                std::cerr << "Trade not connected." << std::endl;
                continue;
            }
            QueryOrderRequest req;
            req.request_id = nextOrderId++;
            req.order_id = 1000;
            tradeApi.QueryOrder(req);
            std::cout << "QueryOrder sent." << std::endl;
        }
        else if (choice == "4") {
            // Connect QuoteGateway (control :12346, data :12347)
            if (quoteApi.IsConnected()) {
                std::cout << "Quote already connected." << std::endl;
                continue;
            }
            int ret = quoteApi.Connect("127.0.0.1", 12346);
            if (ret != 0) {
                std::cerr << "Failed to connect QuoteGateway." << std::endl;
                continue;
            }
            // Subscribe to topics
            quoteApi.Subscribe("notice");
            quoteApi.Subscribe("market.BTC-USDT");
            std::cout << "Quote connected & subscribed to 'notice' + 'market.BTC-USDT'."
                      << std::endl;
        }
        else if (choice == "5") {
            if (!quoteSpi.connected_) {
                std::cerr << "Quote not connected." << std::endl;
                continue;
            }
            std::cout << "\nListening for quote data..." << std::endl;
            std::cout << "(Data will appear above. Press Enter to stop.)" << std::endl;
            std::cin.get();
        }
        else if (choice == "6") {
            tradeApi.Disconnect();
            quoteApi.Disconnect();
            tradeSpi.connected_ = false;
            quoteSpi.connected_ = false;
            std::cout << "All disconnected." << std::endl;
        }
        else if (choice == "0") {
            std::cout << "Exiting..." << std::endl;
            running = false;
        }
        else {
            std::cout << "Invalid choice." << std::endl;
        }
    }

    tradeApi.Disconnect();
    quoteApi.Disconnect();
    return 0;
}
