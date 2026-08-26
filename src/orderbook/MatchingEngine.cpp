#include <algorithm>
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
    if (order.quantity == 0)
    {
        std::cerr << "Invalid order: quantity must be > 0\n";
        return;
    }

    if (order.type == OrderType::LIMIT && order.price <= 0)
    {
        std::cerr << "Invalid order: limit price must be > 0\n";
        return;
    }

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
    }
    else if (order.quantity > 0)
    {
        std::cerr << "Unfilled market order quantity discarded: " << order.quantity << "\n";
    }
}

// ------------------------------------------------------------
// Cancel Order
// ------------------------------------------------------------
void MatchingEngine::cancelOrder(OrderId orderId)
{
    auto orderToInstrumentIt = orderToInstrument.find(orderId);
    if (orderToInstrumentIt == orderToInstrument.end())
    {
        std::cerr << "Order not found: " << orderId << "\n";
        return;
    }

    uint32_t instrumentId = orderToInstrumentIt->second;

    auto instrumentBookIt = books.find(instrumentId);
    if (instrumentBookIt == books.end())
    {
        std::cerr << "OrderBook not found for instrument: " << instrumentId << "\n";
        return;
    }

    instrumentBookIt->second.cancelOrder(orderId);
    orderToInstrument.erase(orderToInstrumentIt);
}

void MatchingEngine::printAllBooks() const
{
    for (const auto& [instrumentId, book] : books)
    {
        std::cout << "\n=== Instrument: " << instrumentId << " ===\n";
        book.print();
    }
}

void MatchingEngine::setTradeHandler(std::function<void(const Trade&)> handler)
{
    tradeHandler = std::move(handler);
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

    if (tradeHandler)
    {
        tradeHandler(trade);
    }

    std::cout << "TRADE | "
              << "BuyerId: " << trade.buyOrderId
              << " SellerId: " << trade.sellOrderId
              << " Price: " << trade.price
              << " Qty: " << trade.quantity
              << "\n";
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
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
