#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/Order.h"
#include "core/Trade.h"
#include "orderbook/OrderBook.h"

struct MatchingEngineMetrics
{
    uint64_t acceptedOrders = 0;
    uint64_t rejectedOrders = 0;
    uint64_t cancelRequests = 0;
    uint64_t successfulCancels = 0;
    uint64_t rejectedCancels = 0;
    uint64_t tradesGenerated = 0;
    uint64_t openOrders = 0;
    uint64_t lastOrderLatencyNs = 0;
    uint64_t maxOrderLatencyNs = 0;
};

// ------------------------------------------------------------
// MatchingEngine
// - Applies matching logic on top of OrderBook
// - Handles price-time priority
// ------------------------------------------------------------
class MatchingEngine {
public:
    MatchingEngine() = default;

    // Entry point: process a new incoming order
    void processOrder(Order order);

    // Cancel existing order
    void cancelOrder(OrderId orderId);

    void printAllBooks() const;

    // For testing
    const std::vector<Trade>& getTrades() const;
    void clearTrades();
    std::vector<Order> getOpenOrders() const;
    void restoreOpenOrders(const std::vector<Order>& orders);
    MatchingEngineMetrics getMetrics() const;

    // Trade callback hook for services / future publishers
    void setTradeHandler(std::function<void(const Trade&)> handler);

    // Last traded price callback hook for market data broadcasters
    void setLtpHandler(std::function<void(uint32_t, Price, uint64_t)> handler);

    // Book snapshot callback hook for services / future publishers
    void setBookHandler(std::function<void(uint32_t, std::optional<Price>, std::optional<Price>, uint64_t)> handler);
    void setTradeLoggingEnabled(bool enabled);

private:
    // For each instrument, we maintain a separate order book
    std::unordered_map<uint32_t, OrderBook> books;
    std::unordered_map<OrderId, uint32_t> orderToInstrument;
    std::vector<Trade> trades;
    std::function<void(const Trade&)> tradeHandler;
    std::function<void(uint32_t, Price, uint64_t)> ltpHandler;
    std::function<void(uint32_t, std::optional<Price>, std::optional<Price>, uint64_t)> bookHandler;
    MatchingEngineMetrics metrics;
    bool tradeLoggingEnabled = false;

    // ------------------- Matching Logic -------------------

    // Match incoming BUY order against asks
    void matchBuy(Order& buyOrder, OrderBook& book);

    // Match incoming SELL order against bids
    void matchSell(Order& sellOrder, OrderBook& book);

    // ------------------- Trade Handling -------------------

    // Called whenever a trade happens
    void onTrade(const Trade& trade);

    void publishBookSnapshot(uint32_t instrumentId);

    // ------------------- Helpers -------------------

    bool validateOrder(const Order& order) const;
    void recordOrderLatency(uint64_t latencyNs);
    bool canMatchBuy(Price buyPrice, Price bestAsk) const;
    bool canMatchSell(Price sellPrice, Price bestBid) const;
};
