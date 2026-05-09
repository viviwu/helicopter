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
#include <unistd.h>

#include "server/Gateway.h"
#include "server/AuthHandler.h"
#include "common/ThreadPool.h"

using namespace gateway;

namespace {
    Gateway*    g_gateway = nullptr;
    ThreadPool* g_pool    = nullptr;
}

static void signalHandler(int /*sig*/) {
    std::cout << "\nReceived signal, shutting down...\n";
    if (g_gateway) {
        g_gateway->Stop();
    }
    // pool shutdown 交由 main() 处理，避免信号处理函数中阻塞过久
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
              << " workers). Press Ctrl+C to stop.\n";

    pause();  // 等待信号

    g_gateway->Stop();     // 幂等（信号处理函数可能已调用）
    pool.Shutdown();       // 等待所有 client 任务完成，安全释放 handler
    g_gateway->Release();

    std::cout << "Gateway server stopped.\n";
    return 0;
}
