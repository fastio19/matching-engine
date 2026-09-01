#include <gtest/gtest.h>

#include <vector>

#include "marketdata/kafka/KafkaSchema.h"
#include "marketdata/kafka/KafkaTradePublishing.h"

namespace
{
class FakeMarketDataPublisher : public IMarketDataPublisher
{
public:
    void publishTrade(const Trade& trade) override
    {
        publishedTrades.push_back(trade);
    }

    void publishBookUpdate(uint32_t,
                           std::optional<Price>,
                           std::optional<Price>,
                           uint64_t) override
    {
    }

    std::vector<Trade> publishedTrades;
};
}

TEST(KafkaTradePublishingTest, PublishesMatchingTradesThroughTradeHandler)
{
    MatchingEngine engine;
    FakeMarketDataPublisher publisher;

    wireTradePublishing(engine, publisher);

    engine.processOrder(Order{1, 101.0, 50, Side::SELL, OrderType::LIMIT, Market::NSE, 1724670000000ULL, 1});
    engine.processOrder(Order{2, 105.0, 50, Side::BUY, OrderType::LIMIT, Market::NSE, 1724670000001ULL, 1});

    ASSERT_EQ(publisher.publishedTrades.size(), 1u);
    const Trade& trade = publisher.publishedTrades.front();
    EXPECT_EQ(trade.buyOrderId, 2u);
    EXPECT_EQ(trade.sellOrderId, 1u);
    EXPECT_EQ(trade.instrumentId, 1u);
    EXPECT_DOUBLE_EQ(trade.price, 101.0);
    EXPECT_EQ(trade.quantity, 50u);
}
