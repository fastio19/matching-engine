#include <gtest/gtest.h>
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