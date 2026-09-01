#include <gtest/gtest.h>

#include <limits>

#include "orderbook/MatchingEngine.h"
#include "core/Order.h"
#include "core/Types.h"
#include "utils/TimeUtils.h"

// ---------- Helper to create orders ----------
static Order createOrder(
    uint64_t id,
    double price,
    uint32_t qty,
    Side side,
    uint32_t instrumentId,
    uint64_t ts)
{
    return Order{
        id,
        price,
        qty,
        side,
        OrderType::LIMIT,
        Market::NSE,
        ts,
        instrumentId
    };
}

static Order createMarketOrder(
    uint64_t id,
    uint32_t qty,
    Side side,
    uint32_t instrumentId,
    uint64_t ts)
{
    return Order{
        id,
        0,
        qty,
        side,
        OrderType::MARKET,
        Market::NSE,
        ts,
        instrumentId
    };
}

// ---------- Test Fixture ----------
class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngine engine;
    uint64_t ts = 1000;

    void SetUp() override {
        engine.clearTrades();   // ensure clean state
    }
};



// ==========================================================
// ✅ FIFO TEST
// ==========================================================
TEST_F(MatchingEngineTest, FIFO_SamePrice)
{
    engine.processOrder(createOrder(1, 100, 50, Side::SELL, 1, ts));
    engine.processOrder(createOrder(2, 100, 30, Side::SELL, 1, ts + 1));

    engine.processOrder(createOrder(3, 100, 70, Side::BUY, 1, ts + 2));

    const auto& trades = engine.getTrades();

    ASSERT_EQ(trades.size(), 2);

    EXPECT_EQ(trades[0].sellOrderId, 1);
    EXPECT_EQ(trades[0].quantity, 50);

    EXPECT_EQ(trades[1].sellOrderId, 2);
    EXPECT_EQ(trades[1].quantity, 20);
}



// ==========================================================
// ✅ PARTIAL FILL TEST
// ==========================================================
TEST_F(MatchingEngineTest, PartialFill)
{
    engine.processOrder(createOrder(1, 100, 50, Side::SELL, 1, ts));
    engine.processOrder(createOrder(2, 100, 20, Side::BUY, 1, ts + 1));

    const auto& trades = engine.getTrades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 20);
}



// ==========================================================
// ✅ FULL FILL TEST
// ==========================================================
TEST_F(MatchingEngineTest, FullFill)
{
    engine.processOrder(createOrder(1, 100, 50, Side::SELL, 1, ts));
    engine.processOrder(createOrder(2, 100, 50, Side::BUY, 1, ts + 1));

    const auto& trades = engine.getTrades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 50);
}



// ==========================================================
// ✅ MULTI-LEVEL MATCHING (PRICE PRIORITY)
// ==========================================================
TEST_F(MatchingEngineTest, MultiLevelMatching)
{
    engine.processOrder(createOrder(1, 101, 50, Side::SELL, 1, ts));
    engine.processOrder(createOrder(2, 102, 30, Side::SELL, 1, ts + 1));

    engine.processOrder(createOrder(3, 105, 70, Side::BUY, 1, ts + 2));

    const auto& trades = engine.getTrades();

    ASSERT_EQ(trades.size(), 2);

    EXPECT_EQ(trades[0].price, 101);
    EXPECT_EQ(trades[1].price, 102);
}



// ==========================================================
// ✅ CANCEL ORDER TEST
// ==========================================================
TEST_F(MatchingEngineTest, CancelOrder)
{
    engine.processOrder(createOrder(1, 100, 50, Side::BUY, 1, ts));
    engine.processOrder(createOrder(2, 100, 30, Side::BUY, 1, ts + 1));

    engine.cancelOrder(2);

    // Try matching — only order 1 should remain
    engine.processOrder(createOrder(3, 100, 50, Side::SELL, 1, ts + 2));

    const auto& trades = engine.getTrades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].buyOrderId, 1);
}



// ==========================================================
// ✅ CANCEL NON-EXISTING ORDER
// ==========================================================
TEST_F(MatchingEngineTest, CancelNonExisting)
{
    EXPECT_NO_THROW(engine.cancelOrder(999));
}



// ==========================================================
// ✅ MULTI-INSTRUMENT ISOLATION
// ==========================================================
TEST_F(MatchingEngineTest, MultiInstrumentIsolation)
{
    // Instrument 1
    engine.processOrder(createOrder(1, 100, 50, Side::SELL, 1, ts));
    engine.processOrder(createOrder(2, 100, 50, Side::BUY, 1, ts + 1));

    // Instrument 2 (should NOT match with instrument 1)
    engine.processOrder(createOrder(3, 200, 40, Side::SELL, 2, ts + 2));

    const auto& trades = engine.getTrades();

    ASSERT_EQ(trades.size(), 1); // only instrument 1 matched
}

// ==========================================================
// ✅ MARKET ORDER TEST
// ==========================================================
TEST_F(MatchingEngineTest, MarketOrderConsumesAvailableLiquidity)
{
    engine.processOrder(createOrder(1, 100, 50, Side::SELL, 1, ts));
    engine.processOrder(createMarketOrder(2, 70, Side::BUY, 1, ts + 1));

    const auto& trades = engine.getTrades();

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 50);
    EXPECT_EQ(trades[0].price, 100);
}

// ==========================================================
// ✅ MARKET ORDER DOES NOT REST
// ==========================================================
TEST_F(MatchingEngineTest, MarketOrderDoesNotRestOnBook)
{
    engine.processOrder(createMarketOrder(1, 20, Side::BUY, 1, ts));
    engine.processOrder(createOrder(2, 100, 20, Side::SELL, 1, ts + 1));

    const auto& trades = engine.getTrades();

    EXPECT_TRUE(trades.empty());
}

// ==========================================================
// ✅ TRADE HANDLER TEST
// ==========================================================
TEST_F(MatchingEngineTest, TradeHandlerReceivesTrades)
{
    bool called = false;
    uint32_t observedQty = 0;

    engine.setTradeHandler([&](const Trade& trade)
    {
        called = true;
        observedQty = trade.quantity;
    });

    engine.processOrder(createOrder(1, 100, 10, Side::SELL, 1, ts));
    engine.processOrder(createOrder(2, 100, 10, Side::BUY, 1, ts + 1));

    EXPECT_TRUE(called);
    EXPECT_EQ(observedQty, 10);
}

// ==========================================================
// ✅ ORDER VALIDATION TESTS
// ==========================================================
TEST_F(MatchingEngineTest, RejectsInvalidOrdersBeforeMatching)
{
    engine.processOrder(createOrder(1, 100, 0, Side::BUY, 1, ts));
    engine.processOrder(createOrder(2, 100, 10, Side::BUY, 0, ts + 1));
    engine.processOrder(createOrder(3, 0, 10, Side::BUY, 1, ts + 2));
    engine.processOrder(createOrder(4, std::numeric_limits<double>::quiet_NaN(), 10, Side::BUY, 1, ts + 3));
    engine.processOrder(Order{5, 100, 10, static_cast<Side>(99), OrderType::LIMIT, Market::NSE, ts + 4, 1});
    engine.processOrder(Order{6, 100, 10, Side::BUY, static_cast<OrderType>(99), Market::NSE, ts + 5, 1});
    engine.processOrder(Order{7, 100, 10, Side::BUY, OrderType::LIMIT, static_cast<Market>(99), ts + 6, 1});

    engine.processOrder(createOrder(8, 100, 10, Side::SELL, 1, ts + 7));

    EXPECT_TRUE(engine.getTrades().empty());
}

TEST_F(MatchingEngineTest, RejectsDuplicateActiveOrderId)
{
    engine.processOrder(createOrder(1, 100, 10, Side::BUY, 1, ts));

    engine.processOrder(createOrder(1, 100, 10, Side::SELL, 1, ts + 1));
    EXPECT_TRUE(engine.getTrades().empty());

    engine.processOrder(createOrder(2, 100, 10, Side::SELL, 1, ts + 2));

    const auto& trades = engine.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].buyOrderId, 1);
    EXPECT_EQ(trades[0].sellOrderId, 2);
}

TEST_F(MatchingEngineTest, RecordsMatchingMetrics)
{
    engine.processOrder(createOrder(1, 100, 50, Side::BUY, 1, ts));
    engine.processOrder(createOrder(2, 100, 20, Side::SELL, 1, ts + 1));
    engine.processOrder(createOrder(3, 100, 0, Side::BUY, 1, ts + 2));
    engine.cancelOrder(999);

    const MatchingEngineMetrics metrics = engine.getMetrics();

    EXPECT_EQ(metrics.acceptedOrders, 2);
    EXPECT_EQ(metrics.rejectedOrders, 1);
    EXPECT_EQ(metrics.cancelRequests, 1);
    EXPECT_EQ(metrics.successfulCancels, 0);
    EXPECT_EQ(metrics.rejectedCancels, 1);
    EXPECT_EQ(metrics.tradesGenerated, 1);
    EXPECT_EQ(metrics.openOrders, 1);
    EXPECT_GT(metrics.lastOrderLatencyNs, 0);
    EXPECT_GE(metrics.maxOrderLatencyNs, metrics.lastOrderLatencyNs);
}