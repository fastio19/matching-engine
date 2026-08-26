#include <iostream>
#include "orderbook/MatchingEngine.h"
#include "utils/TimeUtils.h"
#include "utils/IdGenerator.h"
using namespace std;

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
MatchingEngine::MatchingEngine()
{
}

// ------------------------------------------------------------
// Process Incoming Order
// ------------------------------------------------------------
void MatchingEngine::processOrder(Order order)
{
    if (order.quantity <= 0 || order.price <= 0)
    {
        std::cerr << "Invalid order\n";
        return;
    }

    auto& book = books[order.instrumentId];

    if (order.side == Side::BUY)
    {
        matchBuy(order,book);
    }
    else    
    {
        matchSell(order,book);
    }

    // If still remaining → add to book
    if (order.quantity > 0)
    {
        book.addOrder(order);
        orderToInstrument[order.id] = order.instrumentId;
    }
}

// ------------------------------------------------------------
// Cancel Order
// ------------------------------------------------------------
void MatchingEngine::cancelOrder(OrderId orderId)
{
    auto orderToInstrumentIt  = orderToInstrument.find(orderId);
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

// ------------------------------------------------------------
// Match BUY Order
// ------------------------------------------------------------
void MatchingEngine::matchBuy(Order& buyOrder, OrderBook& book)
{
    while (buyOrder.quantity > 0 && book.hasAsks())
    {
        Price bestAskPrice = book.getBestAsk();

        if (!canMatchBuy(buyOrder.price, bestAskPrice))
            break;

        Order& sellOrder = book.getBestAskOrder(); // FIFO at best price

        int tradedQty = std::min(buyOrder.quantity, sellOrder.quantity);

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

        // Remove fully filled order
        if (sellOrder.quantity == 0)
        {
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

        if (!canMatchSell(sellOrder.price, bestBidPrice))
            break;

        Order& buyOrder = book.getBestBidOrder(); // FIFO at best price

        int tradedQty = std::min(sellOrder.quantity, buyOrder.quantity);

        // Create trade
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

        // Update quantities
        sellOrder.quantity -= tradedQty;
        buyOrder.quantity -= tradedQty;

        // Remove fully filled order
        if (buyOrder.quantity == 0)
        {
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

    // For now → print
    std::cout << "TRADE | "
              << "BuyerId: " << trade.buyOrderId
              << " SellerId: " << trade.sellOrderId
              << " Price: " << trade.price
              << " Qty: " << trade.quantity
              << "\n";

    // Future:
    // - Publish to market data
    // - Persist to DB
    // - Send to Kafka
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

void MatchingEngine::printAllBooks() const
{
    for (const auto& [instrumentId, book] : books)
    {
        std::cout << "Instrument: " << instrumentId << "\n";
        book.print();
    }
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