#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "api/BrokerApiHandler.h"

class CapturingOrderCommandPublisher : public IOrderCommandPublisher {
public:
    void publishOrderCommand(const KafkaOrderCommandMessage& message) override
    {
        messages.push_back(message);
    }

    std::vector<KafkaOrderCommandMessage> messages;
};

TEST(BrokerApiHandlerTest, AcceptsPlaceOrderAndPublishesKafkaCommand)
{
    CapturingOrderCommandPublisher publisher;
    BrokerApiHandler handler(publisher);

    const BrokerApiResponse response = handler.handleRequest(
        "POST",
        "/orders",
        R"({"orderId":101,"instrumentId":1,"side":"BUY","orderType":"LIMIT","price":105.5,"quantity":100})");

    EXPECT_EQ(response.statusCode, 202);
    ASSERT_EQ(publisher.messages.size(), 1);
    EXPECT_EQ(publisher.messages[0].eventType, KafkaEventTypes::ORDER_PLACE);
    EXPECT_EQ(publisher.messages[0].orderId, 101);
    EXPECT_EQ(publisher.messages[0].instrumentId, 1);
    EXPECT_EQ(publisher.messages[0].side, Side::BUY);
    EXPECT_EQ(publisher.messages[0].orderType, OrderType::LIMIT);
    EXPECT_DOUBLE_EQ(publisher.messages[0].price, 105.5);
    EXPECT_EQ(publisher.messages[0].quantity, 100);
    EXPECT_GT(publisher.messages[0].timestamp, 0);
}

TEST(BrokerApiHandlerTest, AcceptsModifyOrderAndPublishesKafkaCommand)
{
    CapturingOrderCommandPublisher publisher;
    BrokerApiHandler handler(publisher);

    const BrokerApiResponse response = handler.handleRequest(
        "PUT",
        "/orders",
        R"({"orderId":101,"instrumentId":1,"side":"SELL","orderType":"LIMIT","price":106.0,"quantity":40,"timestamp":1724670000000})");

    EXPECT_EQ(response.statusCode, 202);
    ASSERT_EQ(publisher.messages.size(), 1);
    EXPECT_EQ(publisher.messages[0].eventType, KafkaEventTypes::ORDER_MODIFY);
    EXPECT_EQ(publisher.messages[0].side, Side::SELL);
    EXPECT_EQ(publisher.messages[0].timestamp, 1724670000000ULL);
}

TEST(BrokerApiHandlerTest, AcceptsCancelOrderAndPublishesKafkaCommand)
{
    CapturingOrderCommandPublisher publisher;
    BrokerApiHandler handler(publisher);

    const BrokerApiResponse response = handler.handleRequest(
        "DELETE",
        "/orders",
        R"({"orderId":101,"instrumentId":1})");

    EXPECT_EQ(response.statusCode, 202);
    ASSERT_EQ(publisher.messages.size(), 1);
    EXPECT_EQ(publisher.messages[0].eventType, KafkaEventTypes::ORDER_CANCEL);
    EXPECT_EQ(publisher.messages[0].orderId, 101);
    EXPECT_EQ(publisher.messages[0].instrumentId, 1);
}

TEST(BrokerApiHandlerTest, RejectsInvalidLimitOrder)
{
    CapturingOrderCommandPublisher publisher;
    BrokerApiHandler handler(publisher);

    const BrokerApiResponse response = handler.handleRequest(
        "POST",
        "/orders",
        R"({"orderId":101,"instrumentId":1,"side":"BUY","orderType":"LIMIT","price":0,"quantity":100})");

    EXPECT_EQ(response.statusCode, 400);
    EXPECT_TRUE(publisher.messages.empty());
}

TEST(BrokerApiHandlerTest, ReturnsUnavailableWhenKafkaPublishFails)
{
    class FailingPublisher : public IOrderCommandPublisher {
    public:
        void publishOrderCommand(const KafkaOrderCommandMessage&) override
        {
            throw std::runtime_error("kafka unavailable");
        }
    } publisher;

    BrokerApiHandler handler(publisher);

    const BrokerApiResponse response = handler.handleRequest(
        "POST",
        "/orders",
        R"({"orderId":101,"instrumentId":1,"side":"BUY","orderType":"LIMIT","price":105.5,"quantity":100})");

    EXPECT_EQ(response.statusCode, 503);
    const auto body = nlohmann::json::parse(response.body);
    EXPECT_EQ(body.at("status").get<std::string>(), "error");
}

TEST(BrokerApiHandlerTest, ExposesRequestMetrics)
{
    CapturingOrderCommandPublisher publisher;
    BrokerApiHandler handler(publisher);

    handler.handleRequest(
        "POST",
        "/orders",
        R"({"orderId":101,"instrumentId":1,"side":"BUY","orderType":"LIMIT","price":105.5,"quantity":100})");
    handler.handleRequest(
        "POST",
        "/orders",
        R"({"orderId":102,"instrumentId":1,"side":"BUY","orderType":"LIMIT","price":0,"quantity":100})");

    const BrokerApiResponse response = handler.handleRequest("GET", "/metrics", "");
    const auto body = nlohmann::json::parse(response.body);

    EXPECT_EQ(response.statusCode, 200);
    EXPECT_EQ(body.at("totalRequests").get<uint64_t>(), 3);
    EXPECT_EQ(body.at("acceptedCommands").get<uint64_t>(), 1);
    EXPECT_EQ(body.at("rejectedRequests").get<uint64_t>(), 1);
    EXPECT_EQ(body.at("publishFailures").get<uint64_t>(), 0);

    const BrokerApiMetrics metrics = handler.getMetrics();
    EXPECT_EQ(metrics.totalRequests, 3);
    EXPECT_EQ(metrics.acceptedCommands, 1);
}
