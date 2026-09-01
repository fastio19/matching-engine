#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include "core/Order.h"

class OrderBook {
public:
    using OrderList = std::list<Order>;


    OrderBook() = default;
    ~OrderBook() = default;

    // ------------------- Core APIs -------------------
    void addOrder(const Order& order);
    void cancelOrder(const OrderId& orderId);
    void modifyOrder(const Order& order);

    // ------------------- Market Data -------------------

    // Best bid (highest buy price)
    Price getBestBid() const;
    
    // Best ask (lowest sell price)
    Price getBestAsk() const;
    
    // Check if book is empty
    bool empty() const;

    //------------------- Helpers -------------------
    bool hasBids() const;
    bool hasAsks() const;
    Order& getBestAskOrder();
    Order& getBestBidOrder();
    
    void removeBestAskOrder();
    void removeBestBidOrder();
    std::vector<Order> getOpenOrders() const;

    // ------------------- Debug -------------------

    void print() const;
private:
    std::map<Price, OrderList, std::greater<>> bids;    // buyers sorted by price descending
    std::map<Price, OrderList> asks; // sellers sorted by price ascending
    std::unordered_map<OrderId, std::pair<Price, OrderList::iterator>> orderMap;
};