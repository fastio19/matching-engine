#include "orderbook/OrderBook.h"
#include <iostream>

bool OrderBook::hasBids() const
{
    return !bids.empty();
}

bool OrderBook::hasAsks() const
{
    return !asks.empty();
}

Price OrderBook::getBestBid() const
{
    return bids.begin()->first;
}
Price OrderBook::getBestAsk() const
{
    return asks.begin()->first;
}
Order& OrderBook::getBestAskOrder()
{
    return asks.begin()->second.front();
}

Order& OrderBook::getBestBidOrder()
{
    return bids.begin()->second.front();
}
void OrderBook::removeBestAskOrder()
{
    if (asks.empty())
        return;

    auto it = asks.begin();
    auto& orderList = it->second;

    if (orderList.empty())
    {
        asks.erase(it);
        return;
    }
    const OrderId removedOrderId = orderList.front().id;
    orderList.pop_front();
    orderMap.erase(removedOrderId);

    // Remove price level if empty
    if (orderList.empty())
    {
        asks.erase(it);
    }
}

void OrderBook::removeBestBidOrder()
{
    if (bids.empty())
        return;

    auto it = bids.begin();
    auto& orderList = it->second;

    if (orderList.empty())
    {
        bids.erase(it);
        return;
    }

    // Remove oldest order (FIFO)
    const OrderId removedOrderId = orderList.front().id;
    orderList.pop_front();
    orderMap.erase(removedOrderId);

    // If no more orders at this price → remove level
    if (orderList.empty())
    {
        bids.erase(it);
    }
}

std::vector<Order> OrderBook::getOpenOrders() const
{
    std::vector<Order> orders;

    for (const auto& [_, orderList] : bids)
    {
        orders.insert(orders.end(), orderList.begin(), orderList.end());
    }

    for (const auto& [_, orderList] : asks)
    {
        orders.insert(orders.end(), orderList.begin(), orderList.end());
    }

    return orders;
}

bool OrderBook::empty() const
{
    return bids.empty() && asks.empty();
}

void OrderBook::addOrder(const Order& order)
{
    // Decide side
    if (order.side == Side::BUY)
    {
        // Get/create price level
        auto& orderList = bids[order.price];
        // Insert at end (FIFO)
        orderList.push_back(order);
        // Get iterator to newly added order
        auto it = std::prev(orderList.end());
        // Store in orderMap
        orderMap[order.id] = {order.price, it};
    }
    else
    {
        // SELL side
        auto& orderList = asks[order.price];
        orderList.push_back(order);
        auto it = std::prev(orderList.end());
        orderMap[order.id] = {order.price, it};
    }
}

void OrderBook::cancelOrder(const OrderId& orderId)
{
    auto it = orderMap.find(orderId);
    if (it == orderMap.end())
        return;  // not found

    Price price = it->second.first;
    auto listIt = it->second.second;

    // Determine side (you may store side in map too)
    if (listIt->side == Side::BUY)
    {
        auto mapIt = bids.find(price);
        if (mapIt != bids.end())
        {
            auto& orderList = mapIt->second;
            orderList.erase(listIt);

            if (orderList.empty())
                bids.erase(mapIt);
        }
    }
    else
    {
        auto mapIt = asks.find(price);
        if (mapIt != asks.end())
        {
            auto& orderList = mapIt->second;
            orderList.erase(listIt);

            if (orderList.empty())
                asks.erase(mapIt);
        }
    }
    orderMap.erase(it);
}

void OrderBook::modifyOrder(const Order& newOrder)
{
    auto it = orderMap.find(newOrder.id);
    if (it == orderMap.end())
        return;  // order not found

    Price oldPrice = it->second.first;
    auto listIt = it->second.second;

    // Get existing order
    Order& existingOrder = *listIt;

    // -------- Case 1: Price unchanged → update quantity --------
    if (existingOrder.price == newOrder.price &&
        existingOrder.side == newOrder.side)
    {
        existingOrder.quantity = newOrder.quantity;
        return;
    }

    // -------- Case 2: Price or side changed → remove and reinsert --------

    // Remove from current list
    if (existingOrder.side == Side::BUY)
    {
        auto mapIt = bids.find(oldPrice);
        if (mapIt != bids.end())
        {
            auto& orderList = mapIt->second;
            orderList.erase(listIt);

            if (orderList.empty())
                bids.erase(mapIt);
        }
    }
    else
    {
        auto mapIt = asks.find(oldPrice);
        if (mapIt != asks.end())
        {
            auto& orderList = mapIt->second;
            orderList.erase(listIt);

            if (orderList.empty())
                asks.erase(mapIt);
        }
    }

    // Remove from map
    orderMap.erase(it);

    // Reinsert as new order (new priority)
    addOrder(newOrder);
}

void OrderBook::print() const
{
    std::cout << "\n========== ORDER BOOK ==========\n";

    // -------- ASKS (Sell Orders) --------
    std::cout << "ASKS (Sell):\n";
    if (asks.empty())
    {
        std::cout << "  [Empty]\n";
    }
    else
    {
        for (const auto& [price, orderList] : asks)
        {
            uint32_t totalQty = 0;
            for (const auto& order : orderList)
            {
                totalQty += order.quantity;
            }

            std::cout << "  " << price << " | " << totalQty << "\n";
        }
    }

    // -------- BIDS (Buy Orders) --------
    std::cout << "\nBIDS (Buy):\n";
    if (bids.empty())
    {
        std::cout << "  [Empty]\n";
    }
    else
    {
        for (const auto& [price, orderList] : bids)
        {
            uint32_t totalQty = 0;
            for (const auto& order : orderList)
            {
                totalQty += order.quantity;
            }

            std::cout << "  " << price << " | " << totalQty << "\n";
        }
    }

    std::cout << "================================\n";
}