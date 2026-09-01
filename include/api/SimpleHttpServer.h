#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "api/BrokerApiHandler.h"

class SimpleHttpServer {
public:
    SimpleHttpServer(std::string host,
                     uint16_t port,
                     std::function<BrokerApiResponse(const std::string&,
                                                     const std::string&,
                                                     const std::string&)> handler);
    ~SimpleHttpServer();

    SimpleHttpServer(const SimpleHttpServer&) = delete;
    SimpleHttpServer& operator=(const SimpleHttpServer&) = delete;

    void run();
    void stop();

private:
    std::string host;
    uint16_t port;
    std::function<BrokerApiResponse(const std::string&,
                                    const std::string&,
                                    const std::string&)> handler;
    std::atomic<bool> running;
    std::intptr_t serverSocket;

    void initSocket();
    void closeServerSocket();
    void handleClient(std::intptr_t clientSocket);
};
