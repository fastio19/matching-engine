#include <gtest/gtest.h>

#include "marketdata/kafka/KafkaSchema.h"

TEST(KafkaOrderCommandConsumerTest, DispatchesPlaceModifyAndCancel)
{
    KafkaOrderCommandHandlers handlers{};
    bool placeCalled = false;
    bool modifyCalled = false;
    bool cancelCalled = false;

    handlers.onPlace = [&](const Order& order)
    {
        placeCalled = true;
        EXPECT_EQ(order.id, 101);
        EXPECT_EQ(order.instrumentId, 1u);
        EXPECT_EQ(order.side, Side::BUY);
    };

    handlers.onModify = [&](const Order& order)
    {
        modifyCalled = true;
        EXPECT_EQ(order.id, 101);
        EXPECT_EQ(order.quantity, 25u);
    };

    handlers.onCancel = [&](OrderId orderId, uint32_t instrumentId)
    {
        cancelCalled = true;
        EXPECT_EQ(orderId, 101u);
        EXPECT_EQ(instrumentId, 1u);
    };

    dispatchOrderCommandMessage(KafkaOrderCommandMessage{
        KafkaEventTypes::ORDER_PLACE,
        101,
        1,
        Side::BUY,
        OrderType::LIMIT,
        105.5,
        100,
        1724670000000ULL
    }, handlers);

    dispatchOrderCommandMessage(KafkaOrderCommandMessage{
        KafkaEventTypes::ORDER_MODIFY,
        101,
        1,
        Side::BUY,
        OrderType::LIMIT,
        106.0,
        25,
        1724670000001ULL
    }, handlers);

    dispatchOrderCommandMessage(KafkaOrderCommandMessage{
        KafkaEventTypes::ORDER_CANCEL,
        101,
        1,
        Side::BUY,
        OrderType::LIMIT,
        0,
        0,
        1724670000002ULL
    }, handlers);

    EXPECT_TRUE(placeCalled);
    EXPECT_TRUE(modifyCalled);
    EXPECT_TRUE(cancelCalled);
}

TEST(KafkaOrderCommandConsumerTest, MissingHandlerThrows)
{
    KafkaOrderCommandHandlers handlers{};

    EXPECT_THROW(dispatchOrderCommandMessage(KafkaOrderCommandMessage{
        KafkaEventTypes::ORDER_PLACE,
        101,
        1,
        Side::BUY,
        OrderType::LIMIT,
        105.5,
        100,
        1724670000000ULL
    }, handlers), std::runtime_error);
}
