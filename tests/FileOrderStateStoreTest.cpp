#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <unordered_map>

#include "orderbook/MatchingEngine.h"
#include "recovery/FileOrderStateStore.h"

namespace
{
    Order makeOrder(OrderId id,
                    Side side,
                    Price price,
                    uint32_t quantity,
                    uint32_t instrumentId,
                    uint64_t timestamp)
    {
        return Order{
            id,
            price,
            quantity,
            side,
            OrderType::LIMIT,
            Market::NSE,
            timestamp,
            instrumentId
        };
    }

    std::string tempSnapshotPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / name).string();
    }
}

TEST(FileOrderStateStoreTest, SavesAndLoadsOpenOrders)
{
    const std::string path = tempSnapshotPath("matching-engine-state-store-test.json");
    std::remove(path.c_str());

    FileOrderStateStore store(path);
    store.saveOpenOrders({
        makeOrder(1, Side::BUY, 100.0, 50, 11, 1000),
        makeOrder(2, Side::SELL, 101.0, 25, 11, 1001)
    });

    const std::vector<Order> loaded = store.loadOpenOrders();

    ASSERT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded[0].id, 1);
    EXPECT_EQ(loaded[0].side, Side::BUY);
    EXPECT_DOUBLE_EQ(loaded[0].price, 100.0);
    EXPECT_EQ(loaded[0].quantity, 50);
    EXPECT_EQ(loaded[0].instrumentId, 11);
    EXPECT_EQ(loaded[1].id, 2);
    EXPECT_EQ(loaded[1].side, Side::SELL);

    std::remove(path.c_str());
}

TEST(FileOrderStateStoreTest, MissingSnapshotLoadsEmptyState)
{
    const std::string path = tempSnapshotPath("matching-engine-missing-state-store-test.json");
    std::remove(path.c_str());

    FileOrderStateStore store(path);

    EXPECT_TRUE(store.loadOpenOrders().empty());
}

TEST(FileOrderStateStoreTest, MatchingEngineRestoresOpenOrdersWithoutReplayingTrades)
{
    MatchingEngine original;
    original.processOrder(makeOrder(1, Side::BUY, 100.0, 50, 11, 1000));
    original.processOrder(makeOrder(2, Side::SELL, 101.0, 25, 11, 1001));

    MatchingEngine restored;
    restored.restoreOpenOrders(original.getOpenOrders());

    EXPECT_TRUE(restored.getTrades().empty());

    restored.processOrder(makeOrder(3, Side::BUY, 101.0, 20, 11, 1002));

    const auto& trades = restored.getTrades();
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].buyOrderId, 3);
    EXPECT_EQ(trades[0].sellOrderId, 2);
    EXPECT_EQ(trades[0].quantity, 20);
    EXPECT_DOUBLE_EQ(trades[0].price, 101.0);
}

TEST(FileOrderStateStoreTest, MatchingEngineSnapshotReflectsFillsAndCancels)
{
    MatchingEngine engine;
    engine.processOrder(makeOrder(1, Side::BUY, 100.0, 50, 11, 1000));
    engine.processOrder(makeOrder(2, Side::BUY, 99.0, 40, 11, 1001));
    engine.processOrder(makeOrder(3, Side::SELL, 100.0, 30, 11, 1002));
    engine.cancelOrder(2);

    const std::vector<Order> openOrders = engine.getOpenOrders();

    ASSERT_EQ(openOrders.size(), 1);
    EXPECT_EQ(openOrders[0].id, 1);
    EXPECT_EQ(openOrders[0].quantity, 20);
}
