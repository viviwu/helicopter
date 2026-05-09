/**
  ******************************************************************************
  * @file           : client.cpp
  * @author         : vivi wu
  * @brief          : GatewayApi/SDK 交互式命令行菜单示例
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include <iostream>
#include <string>
#include <atomic>

// 业务层仅包含 GatewayApi.h，不暴露 gateway.pb.h
#include "api/GatewayApi.h"

using namespace gateway;

// ---------------------------------------------------------------------------
// 用户自定义Spi：继承 GatewaySpi 并实现异步回调
// ---------------------------------------------------------------------------
class MyGatewaySpi : public GatewaySpi {
public:
    explicit MyGatewaySpi(std::atomic<bool>& doneFlag) : done_(doneFlag) {}

    void OnFrontConnected() override {
        std::cout << "\n[MyGatewaySpi] OnFrontConnected" << std::endl;
    }

    void OnFrontDisconnected(int nReason) override {
        std::cout << "\n[MyGatewaySpi] OnFrontDisconnected, reason=" << nReason << std::endl;
    }

    void OnLogin(const LoginResponse& rsp) override {
        std::cout << "\n[MyGatewaySpi] OnLogin callback received:" << std::endl;
        std::cout << "  request_id : " << rsp.request_id << std::endl;
        if (rsp.error_code == ERR_OK) {
            std::cout << "  error_code : 0 (OK)" << std::endl;
            std::cout << "  error_msg  : " << rsp.error_msg << std::endl;
            std::cout << "  account.user_id  : " << rsp.account.user_id << std::endl;
            std::cout << "  account.username : " << rsp.account.username << std::endl;
            std::cout << "  account.email    : " << rsp.account.email << std::endl;
            std::cout << "  account.role     : " << rsp.account.role << std::endl;
        } else {
            std::cout << "  error_code : " << rsp.error_code << std::endl;
            std::cout << "  error_msg  : " << rsp.error_msg << std::endl;
        }
        done_.store(true);
    }

private:
    std::atomic<bool>& done_;
};

// ---------------------------------------------------------------------------
// 菜单
// ---------------------------------------------------------------------------
static void printMenu() {
    std::cout << "\n========== Gateway SDK Demo ==========" << std::endl;
    std::cout << "1. Login" << std::endl;
    std::cout << "2. Exit" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Please select: ";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    GatewayApi* api = GatewayApi::CreateGatewayApi();
    if (!api) {
        std::cerr << "Failed to create GatewayApi" << std::endl;
        return 1;
    }

    std::atomic<bool> loginDone{false};
    MyGatewaySpi spi(loginDone);
    api->RegisterSpi(&spi);

    uint64_t nextReqId = 1;
    bool running = true;

    while (running) {
        printMenu();
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "1") {
            // Init 时传入 IP、端口、心跳间隔（秒）
            int ret = api->Init("127.0.0.1", 12345, 5);
            if (ret != ERR_OK) {
                std::cerr << "[Main] Init failed, unable to connect to front server" << std::endl;
                continue;
            }

            std::string username, password;
            std::cout << "Username: ";
            std::getline(std::cin, username);
            std::cout << "Password: ";
            std::getline(std::cin, password);

            LoginRequest req;
            req.request_id = nextReqId++;
            req.username   = std::move(username);
            req.password   = std::move(password);

            ret = api->Login(req);
            if (ret == ERR_OK) {
                std::cout << "[Main] Login request sent, request_id=" << req.request_id << std::endl;
            } else if (ret == ERR_NETWORK) {
                std::cerr << "[Main] Login failed: not connected" << std::endl;
            } else {
                std::cerr << "[Main] Login failed: send error" << std::endl;
            }
        }
        else if (choice == "2") {
            std::cout << "[Main] Exiting..." << std::endl;
            api->Release();
            running = false;
        }
        else {
            std::cout << "Invalid choice, please try again." << std::endl;
        }
    }

    return 0;
}
