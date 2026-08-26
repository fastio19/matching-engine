#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "core/Order.h"
#include "core/Trade.h"
#include "orderbook/OrderBook.h"

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

    // Trade callback hook for services / future publishers
    void setTradeHandler(std::function<void(const Trade&)> handler);

private:
    // For each instrument, we maintain a separate order book
    std::unordered_map<uint32_t, OrderBook> books;
    std::unordered_map<OrderId, uint32_t> orderToInstrument;
    std::vector<Trade> trades;
    std::function<void(const Trade&)> tradeHandler;

    // ------------------- Matching Logic -------------------

    // Match incoming BUY order against asks
    void matchBuy(Order& buyOrder, OrderBook& book);

    // Match incoming SELL order against bids
    void matchSell(Order& sellOrder, OrderBook& book);

    // ------------------- Trade Handling -------------------

    // Called whenever a trade happens
    void onTrade(const Trade& trade);

    // ------------------- Helpers -------------------

    bool canMatchBuy(Price buyPrice, Price bestAsk) const;
    bool canMatchSell(Price sellPrice, Price bestBid) const;
};
