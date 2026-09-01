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

    void publishLastTradedPrice(uint32_t instrumentId,
                                Price lastTradedPrice,
                                uint64_t timestamp) override
    {
        ltps.push_back(KafkaLastTradedPriceMessage::fromLastTradedPrice(
            instrumentId,
            lastTradedPrice,
            timestamp));
    }

    void publishBookUpdate(uint32_t,
                           std::optional<Price>,
                           std::optional<Price>,
                           uint64_t) override
    {
    }

    std::vector<KafkaLastTradedPriceMessage> ltps;
};
}

TEST(KafkaLtpPublishingTest, PublishesLastTradedPriceOnTrade)
{
    MatchingEngine engine;
    FakeMarketDataPublisher publisher;

    wireLastTradedPricePublishing(engine, publisher);

    engine.processOrder(Order{1, 101.0, 50, Side::SELL, OrderType::LIMIT, Market::NSE, 1724670000000ULL, 1});
    engine.processOrder(Order{2, 105.0, 50, Side::BUY, OrderType::LIMIT, Market::NSE, 1724670000001ULL, 1});

    ASSERT_EQ(publisher.ltps.size(), 1u);
    const auto& ltp = publisher.ltps.front();
    EXPECT_EQ(ltp.eventType, KafkaEventTypes::LAST_TRADED_PRICE);
    EXPECT_EQ(ltp.instrumentId, 1u);
    EXPECT_DOUBLE_EQ(ltp.lastTradedPrice, 101.0);
    EXPECT_GT(ltp.timestamp, 0ULL);
}
