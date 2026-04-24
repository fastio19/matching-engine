#pragma once

#include "Order.h"
#include "Trade.h"
#include "OrderBook.h"

// ------------------------------------------------------------
// MatchingEngine
// - Applies matching logic on top of OrderBook
// - Handles price-time priority
// ------------------------------------------------------------
class MatchingEngine {
public:
    explicit MatchingEngine(OrderBook& orderBook);

    // Entry point: process a new incoming order
    void processOrder(Order order);

    // Cancel existing order
    void cancelOrder(OrderId orderId);

private:
    //For Each instrument, we maintain a separate order book
    std::unordered_map<uint32_t, OrderBook> books;
    std::unordered_map<OrderId, uint32_t> orderToInstrument;
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