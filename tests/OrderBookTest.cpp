#include <gtest/gtest.h>

#include "orderbook/OrderBook.h"

static Order makeLimitOrder(
    OrderId id,
    Side side,
    double price,
    uint32_t quantity,
    uint32_t instrumentId = 1)
{
    return Order{
        id,
        price,
        quantity,
        side,
        OrderType::LIMIT,
        Market::NSE,
        1000,
        instrumentId
    };
}

TEST(OrderBookTest, AddAndCancelOrder)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::BUY, 100, 50));
    EXPECT_FALSE(book.empty());
    EXPECT_TRUE(book.hasBids());
    EXPECT_FALSE(book.hasAsks());
    EXPECT_DOUBLE_EQ(book.getBestBid(), 100);

    book.cancelOrder(1);
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, ModifyOrderKeepsPriorityOnSamePrice)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::SELL, 101, 50));
    book.addOrder(makeLimitOrder(2, Side::SELL, 101, 30));

    book.modifyOrder(makeLimitOrder(1, Side::SELL, 101, 20));

    EXPECT_DOUBLE_EQ(book.getBestAsk(), 101);
    EXPECT_EQ(book.getBestAskOrder().id, 1);
    EXPECT_EQ(book.getBestAskOrder().quantity, 20);
}

TEST(OrderBookTest, ModifyOrderMovesToNewPriceLevel)
{
    OrderBook book;

    book.addOrder(makeLimitOrder(1, Side::BUY, 100, 50));
    book.addOrder(makeLimitOrder(2, Side::BUY, 99, 40));

    book.modifyOrder(makeLimitOrder(1, Side::BUY, 105, 25));

    EXPECT_DOUBLE_EQ(book.getBestBid(), 105);
    EXPECT_EQ(book.getBestBidOrder().id, 1);
    EXPECT_EQ(book.getBestBidOrder().quantity, 25);
}
