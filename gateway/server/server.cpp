/**
  ******************************************************************************
  * @file           : server.cpp
  * @author         : vivi wu
  * @brief          : Gateway 服务端入口（Gateway 框架 + AuthHandler + ThreadPool）
  * @version        : 0.3.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#include <iostream>
#include <csignal>
#include <string>

#include "server/Gateway.h"
#include "server/AuthHandler.h"
#include "common/ThreadPool.h"

using namespace gateway;

namespace {
    Gateway*    g_gateway = nullptr;
    ThreadPool* g_pool    = nullptr;
    volatile std::sig_atomic_t g_shutdownRequested = 0;
}

static void signalHandler(int /*sig*/) {
    // 信号处理函数中仅设置标志位，实际关闭由 main 线程执行
    g_shutdownRequested = 1;
}

static void printMenu() {
    std::cout << "\n========== Gateway Server Menu ==========\n"
              << "1. Broadcast notification to all clients\n"
              << "2. Shutdown server\n"
              << "=========================================\n"
              << "Please select: ";
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    AuthHandler authHandler;
    ThreadPool pool(4);  // 4 个工作线程，可根据 CPU 核心数调整

    g_gateway = Gateway::Create();
    g_pool    = &pool;
    g_gateway->RegisterHandler(&authHandler);
    g_gateway->SetThreadPool(&pool);
    g_gateway->SetMaxConnections(10000);  // 最大并发连接数
    g_gateway->SetIdleTimeout(60);        // 空闲 60s 自动回收

    if (!g_gateway->Start("0.0.0.0", 12345)) {
        std::cerr << "Failed to start gateway\n";
        return 1;
    }

    std::cout << "Gateway server running with thread pool (" << pool.WorkerCount()
              << " workers).\n";

    // 交互式菜单主循环
    while (!g_shutdownRequested) {
        printMenu();
        std::string choice;
        if (!std::getline(std::cin, choice)) {
            // EOF 或信号中断输入
            break;
        }

        if (choice == "1") {
            std::cout << "Enter broadcast message: ";
            std::string msg;
            if (!std::getline(std::cin, msg)) break;
            g_gateway->BroadcastNotification(msg);
        }
        else if (choice == "2") {
            std::cout << "Shutting down...\n";
            g_shutdownRequested = 1;
        }
        else {
            std::cout << "Invalid choice, please try again.\n";
        }
    }

    std::cout << "\nReceived signal, shutting down...\n";

    g_gateway->Stop();     // 停止接受连接，通知 dispatch 线程退出
    pool.Shutdown();       // 等待所有 client 任务完成，安全释放 handler
    g_gateway->Release();

    std::cout << "Gateway server stopped.\n";
    return 0;
}
