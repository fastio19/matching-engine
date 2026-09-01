#include <gtest/gtest.h>

#include "marketdata/kafka/KafkaSchema.h"

TEST(KafkaSchemaTest, OrderCommandRoundTrip)
{
    Order order{
        101,
        105.5,
        100,
        Side::BUY,
        OrderType::LIMIT,
        Market::NSE,
        1724670000000ULL,
        1
    };

    const auto message = KafkaOrderCommandMessage::fromOrder(order, KafkaEventTypes::ORDER_PLACE);
    const auto payload = toJson(message).dump();
    const auto parsed = parseOrderCommandMessage(payload);

    EXPECT_EQ(parsed.eventType, KafkaEventTypes::ORDER_PLACE);
    EXPECT_EQ(parsed.orderId, order.id);
    EXPECT_EQ(parsed.instrumentId, order.instrumentId);
    EXPECT_EQ(parsed.side, order.side);
    EXPECT_EQ(parsed.orderType, order.type);
    EXPECT_DOUBLE_EQ(parsed.price, order.price);
    EXPECT_EQ(parsed.quantity, order.quantity);
    EXPECT_EQ(parsed.timestamp, order.timestamp);
}

TEST(KafkaSchemaTest, TradeEventRoundTrip)
{
    Trade trade{
        101,
        202,
        1,
        105.0,
        50,
        1724670000001ULL,
        9001
    };

    const auto message = KafkaTradeEventMessage::fromTrade(trade);
    const auto payload = toJson(message).dump();
    const auto parsed = parseTradeEventMessage(payload);

    EXPECT_EQ(parsed.eventType, KafkaEventTypes::TRADE);
    EXPECT_EQ(parsed.tradeId, trade.tradeId);
    EXPECT_EQ(parsed.buyOrderId, trade.buyOrderId);
    EXPECT_EQ(parsed.sellOrderId, trade.sellOrderId);
    EXPECT_EQ(parsed.instrumentId, trade.instrumentId);
    EXPECT_DOUBLE_EQ(parsed.price, trade.price);
    EXPECT_EQ(parsed.quantity, trade.quantity);
    EXPECT_EQ(parsed.timestamp, trade.timestamp);
}

TEST(KafkaSchemaTest, BookUpdateRoundTrip)
{
    const KafkaBookUpdateMessage message{
        KafkaEventTypes::BOOK_UPDATE,
        1,
        100.5,
        101.0,
        1724670000002ULL
    };

    const auto payload = toJson(message).dump();
    const auto parsed = parseBookUpdateMessage(payload);

    EXPECT_EQ(parsed.eventType, KafkaEventTypes::BOOK_UPDATE);
    EXPECT_EQ(parsed.instrumentId, message.instrumentId);
    EXPECT_DOUBLE_EQ(parsed.bestBid, message.bestBid);
    EXPECT_DOUBLE_EQ(parsed.bestAsk, message.bestAsk);
    EXPECT_EQ(parsed.timestamp, message.timestamp);
}

TEST(KafkaSchemaTest, LastTradedPriceRoundTrip)
{
    const KafkaLastTradedPriceMessage message{
        KafkaEventTypes::LAST_TRADED_PRICE,
        1,
        101.25,
        1724670000003ULL
    };

    const auto payload = toJson(message).dump();
    const auto parsed = parseLastTradedPriceMessage(payload);

    EXPECT_EQ(parsed.eventType, KafkaEventTypes::LAST_TRADED_PRICE);
    EXPECT_EQ(parsed.instrumentId, message.instrumentId);
    EXPECT_DOUBLE_EQ(parsed.lastTradedPrice, message.lastTradedPrice);
    EXPECT_EQ(parsed.timestamp, message.timestamp);
}
