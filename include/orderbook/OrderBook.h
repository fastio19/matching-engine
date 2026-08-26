#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include "core/Order.h"

using namespace std;

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

    // ------------------- Debug -------------------

    void print() const;
private:
    map<Price, OrderList,std::greater<>> bids;
    map<Price, OrderList> asks;
    unordered_map<OrderId, std::pair<Price, OrderList::iterator>> orderMap;
};