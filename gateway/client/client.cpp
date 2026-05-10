/**
  ******************************************************************************
  * @file           : client.cpp
  * @author         : vivi wu
  * @brief          : GatewayApi/SDK 交互式命令行菜单示例
  * @version        : 0.2.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include <iostream>
#include <string>
#include <atomic>
#include <csignal>

// 业务层仅包含 GatewayApi.h，不暴露 gateway.pb.h
#include "api/GatewayApi.h"

using namespace gateway;

// ---------------------------------------------------------------------------
// 用户自定义Spi：继承 GatewaySpi 并实现异步回调
// ---------------------------------------------------------------------------
class MyGatewaySpi : public GatewaySpi {
public:
    explicit MyGatewaySpi(std::atomic<bool>& loginDone)
        : loginDone_(loginDone) {}

    void OnFrontConnected() override {
        std::cout << "\n[MyGatewaySpi] OnFrontConnected" << std::endl;
    }

    void OnFrontDisconnected(int nReason) override {
        std::cout << "\n[MyGatewaySpi] OnFrontDisconnected, reason=" << nReason << std::endl;
        connected_.store(false);
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
        loginDone_.store(true);
    }

    void OnBroadcast(const BroadcastNotification& notif) override {
        std::cout << "\n[MyGatewaySpi] >>> BROADCAST received <<<" << std::endl;
        std::cout << "  content   : " << notif.content << std::endl;
        std::cout << "  timestamp : " << notif.timestamp << std::endl;
        std::cout << "Press Enter to continue..." << std::endl;
    }

    std::atomic<bool> connected_{false};

private:
    std::atomic<bool>& loginDone_;
};

// ---------------------------------------------------------------------------
// 菜单
// ---------------------------------------------------------------------------
static void printMenu(bool connected) {
    std::cout << "\n========== Gateway SDK Demo ==========" << std::endl;
    if (connected) {
        std::cout << "Status: Connected" << std::endl;
    } else {
        std::cout << "Status: Disconnected" << std::endl;
    }
    std::cout << "--------------------------------------" << std::endl;
    std::cout << "1. Connect to server" << std::endl;
    std::cout << "2. Login" << std::endl;
    std::cout << "3. Listen for broadcasts (press Enter to stop)" << std::endl;
    std::cout << "4. Disconnect" << std::endl;
    std::cout << "0. Exit" << std::endl;
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
        printMenu(spi.connected_.load());

        std::string choice;
        if (!std::getline(std::cin, choice)) {
            break;
        }

        if (choice == "1") {
            if (spi.connected_.load()) {
                std::cout << "Already connected. Disconnect first (option 4)." << std::endl;
                continue;
            }
            int ret = api->Init("127.0.0.1", 12345, 5);
            if (ret == ERR_OK) {
                spi.connected_ = true;
                std::cout << "Connected to server (DEALER:12345 + SUB:12346)." << std::endl;
            } else {
                std::cerr << "Failed to connect to server." << std::endl;
            }
        }
        else if (choice == "2") {
            if (!spi.connected_.load()) {
                std::cerr << "Not connected. Connect first (option 1)." << std::endl;
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

            loginDone = false;
            int ret = api->Login(req);
            if (ret == ERR_OK) {
                std::cout << "Login request sent, request_id=" << req.request_id << std::endl;
            } else if (ret == ERR_NETWORK) {
                std::cerr << "Login failed: not connected." << std::endl;
            } else {
                std::cerr << "Login failed: send error." << std::endl;
            }
        }
        else if (choice == "3") {
            if (!spi.connected_.load()) {
                std::cerr << "Not connected. Connect first (option 1)." << std::endl;
                continue;
            }
            std::cout << "\nListening for broadcast messages..." << std::endl;
            std::cout << "(Broadcasts will appear above. Press Enter to stop listening.)"
                      << std::endl;
            std::cin.get();  // 阻塞等待 Enter
        }
        else if (choice == "4") {
            if (!spi.connected_.load()) {
                std::cout << "Already disconnected." << std::endl;
                continue;
            }
            api->Release();
            api = GatewayApi::CreateGatewayApi();
            api->RegisterSpi(&spi);
            spi.connected_ = false;
            loginDone = false;
            nextReqId = 1;
            std::cout << "Disconnected." << std::endl;
        }
        else if (choice == "0") {
            std::cout << "Exiting..." << std::endl;
            running = false;
        }
        else {
            std::cout << "Invalid choice." << std::endl;
        }
    }

    api->Release();
    return 0;
}
