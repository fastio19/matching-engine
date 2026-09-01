#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <utility>

#include "orderbook/MatchingEngine.h"
#include "utils/IdGenerator.h"
#include "utils/TimeUtils.h"

// ------------------------------------------------------------
// Process Incoming Order
// ------------------------------------------------------------
void MatchingEngine::processOrder(Order order)
{
    const auto start = std::chrono::steady_clock::now();

    if (!validateOrder(order))
    {
        ++metrics.rejectedOrders;
        const auto end = std::chrono::steady_clock::now();
        recordOrderLatency(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
        return;
    }

    ++metrics.acceptedOrders;

    auto& book = books[order.instrumentId];

    if (order.side == Side::BUY)
    {
        matchBuy(order, book);
    }
    else
    {
        matchSell(order, book);
    }

    if (order.quantity > 0 && order.type == OrderType::LIMIT)
    {
        book.addOrder(order);
        orderToInstrument[order.id] = order.instrumentId;
        ++metrics.openOrders;
    }
    else if (order.quantity > 0)
    {
        std::cerr << "Unfilled market order quantity discarded: " << order.quantity << "\n";
    }
    publishBookSnapshot(order.instrumentId);

    const auto end = std::chrono::steady_clock::now();
    recordOrderLatency(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
}

// ------------------------------------------------------------
// Cancel Order
// ------------------------------------------------------------
void MatchingEngine::cancelOrder(OrderId orderId)
{
    ++metrics.cancelRequests;

    auto orderToInstrumentIt = orderToInstrument.find(orderId);
    if (orderToInstrumentIt == orderToInstrument.end())
    {
        ++metrics.rejectedCancels;
        std::cerr << "Order not found: " << orderId << "\n";
        return;
    }

    uint32_t instrumentId = orderToInstrumentIt->second;

    auto instrumentBookIt = books.find(instrumentId);
    if (instrumentBookIt == books.end())
    {
        ++metrics.rejectedCancels;
        std::cerr << "OrderBook not found for instrument: " << instrumentId << "\n";
        return;
    }

    instrumentBookIt->second.cancelOrder(orderId);
    orderToInstrument.erase(orderToInstrumentIt);
    ++metrics.successfulCancels;
    if (metrics.openOrders > 0)
    {
        --metrics.openOrders;
    }
    publishBookSnapshot(instrumentId);
}

void MatchingEngine::printAllBooks() const
{
    for (const auto& [instrumentId, book] : books)
    {
        std::cout << "\n=== Instrument: " << instrumentId << " ===\n";
        book.print();
    }
}

std::vector<Order> MatchingEngine::getOpenOrders() const
{
    std::vector<Order> openOrders;

    for (const auto& [_, book] : books)
    {
        const std::vector<Order> bookOrders = book.getOpenOrders();
        openOrders.insert(openOrders.end(), bookOrders.begin(), bookOrders.end());
    }

    return openOrders;
}

void MatchingEngine::restoreOpenOrders(const std::vector<Order>& orders)
{
    books.clear();
    orderToInstrument.clear();
    trades.clear();
    metrics = MatchingEngineMetrics{};

    for (const Order& order : orders)
    {
        if (!validateOrder(order))
        {
            throw std::runtime_error("Invalid order in recovery snapshot");
        }

        if (order.type != OrderType::LIMIT)
        {
            throw std::runtime_error("Recovery snapshot can only contain resting limit orders");
        }

        auto& book = books[order.instrumentId];
        book.addOrder(order);
        orderToInstrument[order.id] = order.instrumentId;
    }

    metrics.openOrders = orders.size();
}

MatchingEngineMetrics MatchingEngine::getMetrics() const
{
    return metrics;
}

void MatchingEngine::setTradeHandler(std::function<void(const Trade&)> handler)
{
    tradeHandler = std::move(handler);
}

void MatchingEngine::setLtpHandler(std::function<void(uint32_t, Price, uint64_t)> handler)
{
    ltpHandler = std::move(handler);
}

void MatchingEngine::setBookHandler(std::function<void(uint32_t, std::optional<Price>, std::optional<Price>, uint64_t)> handler)
{
    bookHandler = std::move(handler);
}

void MatchingEngine::setTradeLoggingEnabled(bool enabled)
{
    tradeLoggingEnabled = enabled;
}

// ------------------------------------------------------------
// Match BUY Order
// ------------------------------------------------------------
void MatchingEngine::matchBuy(Order& buyOrder, OrderBook& book)
{
    while (buyOrder.quantity > 0 && book.hasAsks())
    {
        Price bestAskPrice = book.getBestAsk();

        if (buyOrder.type == OrderType::LIMIT &&
            !canMatchBuy(buyOrder.price, bestAskPrice))
        {
            break;
        }

        Order& sellOrder = book.getBestAskOrder();

        uint32_t tradedQty = std::min(buyOrder.quantity, sellOrder.quantity);

        Trade trade(
            buyOrder.id,
            sellOrder.id,
            buyOrder.instrumentId,
            bestAskPrice,
            tradedQty,
            TimeUtils::getCurrentTime(),
            IdGenerator::generateId()
        );

        onTrade(trade);

        buyOrder.quantity -= tradedQty;
        sellOrder.quantity -= tradedQty;

        if (sellOrder.quantity == 0)
        {
            orderToInstrument.erase(sellOrder.id);
            if (metrics.openOrders > 0)
            {
                --metrics.openOrders;
            }
            book.removeBestAskOrder();
        }
    }
}

// ------------------------------------------------------------
// Match SELL Order
// ------------------------------------------------------------
void MatchingEngine::matchSell(Order& sellOrder, OrderBook& book)
{
    while (sellOrder.quantity > 0 && book.hasBids())
    {
        Price bestBidPrice = book.getBestBid();

        if (sellOrder.type == OrderType::LIMIT &&
            !canMatchSell(sellOrder.price, bestBidPrice))
        {
            break;
        }

        Order& buyOrder = book.getBestBidOrder();

        uint32_t tradedQty = std::min(sellOrder.quantity, buyOrder.quantity);

        Trade trade(
            buyOrder.id,
            sellOrder.id,
            buyOrder.instrumentId,
            bestBidPrice,
            tradedQty,
            TimeUtils::getCurrentTime(),
            IdGenerator::generateId()
        );

        onTrade(trade);

        sellOrder.quantity -= tradedQty;
        buyOrder.quantity -= tradedQty;

        if (buyOrder.quantity == 0)
        {
            orderToInstrument.erase(buyOrder.id);
            if (metrics.openOrders > 0)
            {
                --metrics.openOrders;
            }
            book.removeBestBidOrder();
        }
    }
}

// ------------------------------------------------------------
// Trade Callback
// ------------------------------------------------------------
void MatchingEngine::onTrade(const Trade& trade)
{
    trades.push_back(trade);
    ++metrics.tradesGenerated;

    if (tradeHandler)
    {
        tradeHandler(trade);
    }

    if (ltpHandler)
    {
        ltpHandler(trade.instrumentId, trade.price, trade.timestamp);
    }

    if (tradeLoggingEnabled)
    {
        std::cout << "TRADE | "
                  << "BuyerId: " << trade.buyOrderId
                  << " SellerId: " << trade.sellOrderId
                  << " Price: " << trade.price
                  << " Qty: " << trade.quantity
                  << "\n";
    }
}

void MatchingEngine::publishBookSnapshot(uint32_t instrumentId)
{
    auto bookIt = books.find(instrumentId);
    if (bookIt == books.end() || !bookHandler)
    {
        return;
    }

    std::optional<Price> bestBid;
    std::optional<Price> bestAsk;

    if (bookIt->second.hasBids())
    {
        bestBid = bookIt->second.getBestBid();
    }

    if (bookIt->second.hasAsks())
    {
        bestAsk = bookIt->second.getBestAsk();
    }

    bookHandler(instrumentId,
                bestBid,
                bestAsk,
                TimeUtils::getCurrentTime());
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
bool MatchingEngine::validateOrder(const Order& order) const
{
    if (order.id == 0)
    {
        std::cerr << "Invalid order: order id must be > 0\n";
        return false;
    }

    if (orderToInstrument.find(order.id) != orderToInstrument.end())
    {
        std::cerr << "Invalid order: duplicate active order id " << order.id << "\n";
        return false;
    }

    if (order.instrumentId == 0)
    {
        std::cerr << "Invalid order: instrument id must be > 0\n";
        return false;
    }

    if (order.quantity == 0)
    {
        std::cerr << "Invalid order: quantity must be > 0\n";
        return false;
    }

    if (order.side != Side::BUY && order.side != Side::SELL)
    {
        std::cerr << "Invalid order: side is unknown\n";
        return false;
    }

    if (order.type != OrderType::LIMIT && order.type != OrderType::MARKET)
    {
        std::cerr << "Invalid order: order type is unknown\n";
        return false;
    }

    if (order.market != Market::NSE && order.market != Market::BSE)
    {
        std::cerr << "Invalid order: market is unknown\n";
        return false;
    }

    if (order.type == OrderType::LIMIT && (!std::isfinite(order.price) || order.price <= 0))
    {
        std::cerr << "Invalid order: limit price must be finite and > 0\n";
        return false;
    }

    return true;
}

void MatchingEngine::recordOrderLatency(uint64_t latencyNs)
{
    metrics.lastOrderLatencyNs = latencyNs;
    metrics.maxOrderLatencyNs = std::max(metrics.maxOrderLatencyNs, latencyNs);
}

bool MatchingEngine::canMatchBuy(Price buyPrice, Price bestAsk) const
{
    return buyPrice >= bestAsk;
}

bool MatchingEngine::canMatchSell(Price sellPrice, Price bestBid) const
{
    return sellPrice <= bestBid;
}

// ------------------------------------------------------------
// For Testing
// ------------------------------------------------------------
const std::vector<Trade>& MatchingEngine::getTrades() const
{
    return trades;
}

void MatchingEngine::clearTrades()
{
    trades.clear();
}
