#include <gtest/gtest.h>

#include <vector>

#include "marketdata/kafka/KafkaSchema.h"
#include "marketdata/kafka/KafkaTradePublishing.h"

namespace
{
class FakeMarketDataPublisher : public IMarketDataPublisher
{
public:
    void publishTrade(const Trade&) override
    {
    }

    void publishBookUpdate(uint32_t instrumentId,
                           std::optional<Price> bestBid,
                           std::optional<Price> bestAsk,
                           uint64_t timestamp) override
    {
        bookUpdates.push_back(KafkaBookUpdateMessage{
            KafkaEventTypes::BOOK_UPDATE,
            instrumentId,
            bestBid.value_or(0.0),
            bestAsk.value_or(0.0),
            timestamp
        });
    }

    std::vector<KafkaBookUpdateMessage> bookUpdates;
};
}

TEST(KafkaBookPublishingTest, PublishesBookSnapshotsOnOrderChanges)
{
    MatchingEngine engine;
    FakeMarketDataPublisher publisher;

    wireBookPublishing(engine, publisher);

    engine.processOrder(Order{1, 101.0, 50, Side::SELL, OrderType::LIMIT, Market::NSE, 1724670000000ULL, 1});
    ASSERT_EQ(publisher.bookUpdates.size(), 1u);
    EXPECT_EQ(publisher.bookUpdates.back().instrumentId, 1u);
    EXPECT_DOUBLE_EQ(publisher.bookUpdates.back().bestBid, 0.0);
    EXPECT_DOUBLE_EQ(publisher.bookUpdates.back().bestAsk, 101.0);

    engine.processOrder(Order{2, 105.0, 50, Side::BUY, OrderType::LIMIT, Market::NSE, 1724670000001ULL, 1});
    ASSERT_EQ(publisher.bookUpdates.size(), 2u);
    EXPECT_DOUBLE_EQ(publisher.bookUpdates.back().bestBid, 0.0);
    EXPECT_DOUBLE_EQ(publisher.bookUpdates.back().bestAsk, 0.0);
}
