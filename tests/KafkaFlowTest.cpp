#include <gtest/gtest.h>

#include "marketdata/kafka/KafkaSchema.h"
#include "orderbook/MatchingEngine.h"

TEST(KafkaFlowTest, CommandDispatchToTradeEvent)
{
    MatchingEngine engine;
    std::string tradePayload;

    engine.setTradeHandler([&](const Trade& trade)
    {
        tradePayload = toJson(KafkaTradeEventMessage::fromTrade(trade)).dump();
    });

    KafkaOrderCommandHandlers handlers{};
    handlers.onPlace = [&](const Order& order)
    {
        engine.processOrder(order);
    };

    dispatchOrderCommandMessage(KafkaOrderCommandMessage{
        KafkaEventTypes::ORDER_PLACE,
        1,
        1,
        Side::SELL,
        OrderType::LIMIT,
        101.0,
        50,
        1724670000000ULL
    }, handlers);

    dispatchOrderCommandMessage(KafkaOrderCommandMessage{
        KafkaEventTypes::ORDER_PLACE,
        2,
        1,
        Side::BUY,
        OrderType::LIMIT,
        105.0,
        50,
        1724670000001ULL
    }, handlers);

    ASSERT_FALSE(tradePayload.empty());

    const auto parsed = parseTradeEventMessage(tradePayload);
    EXPECT_EQ(parsed.eventType, KafkaEventTypes::TRADE);
    EXPECT_EQ(parsed.buyOrderId, 2u);
    EXPECT_EQ(parsed.sellOrderId, 1u);
    EXPECT_EQ(parsed.instrumentId, 1u);
    EXPECT_DOUBLE_EQ(parsed.price, 101.0);
    EXPECT_EQ(parsed.quantity, 50u);
}
