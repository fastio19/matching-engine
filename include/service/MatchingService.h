#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <variant>

#include "core/Order.h"
#include "orderbook/MatchingEngine.h"

struct PlaceOrderCommand {
    Order order;
};

struct CancelOrderCommand {
    OrderId orderId;
};

using MatchingCommand = std::variant<PlaceOrderCommand, CancelOrderCommand>;

class MatchingService {
public:
    MatchingService() = default;
    ~MatchingService();

    MatchingService(const MatchingService&) = delete;
    MatchingService& operator=(const MatchingService&) = delete;

    void start();
    void stop();

    void submitOrder(const Order& order);
    void submitCancel(OrderId orderId);

    void waitUntilIdle();

    bool isRunning() const;

    MatchingEngine& engine();
    const MatchingEngine& engine() const;

private:
    void enqueue(MatchingCommand command);
    void run();

    MatchingEngine matchingEngine;
    std::thread worker;

    mutable std::mutex mutex;
    std::condition_variable commandCv;
    std::condition_variable idleCv;
    std::queue<MatchingCommand> commands;

    bool running = false;
    bool stopping = false;
    std::size_t activeCommands = 0;
};
