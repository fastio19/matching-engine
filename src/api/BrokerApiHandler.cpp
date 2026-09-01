#include "api/BrokerApiHandler.h"

#include <cmath>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "utils/TimeUtils.h"

namespace
{
    BrokerApiResponse jsonResponse(int statusCode, const nlohmann::json& body)
    {
        return BrokerApiResponse{statusCode, body.dump()};
    }

    Side parseSide(const std::string& value)
    {
        if (value == "BUY")
        {
            return Side::BUY;
        }
        if (value == "SELL")
        {
            return Side::SELL;
        }
        throw std::runtime_error("side must be BUY or SELL");
    }

    OrderType parseOrderType(const std::string& value)
    {
        if (value == "LIMIT")
        {
            return OrderType::LIMIT;
        }
        if (value == "MARKET")
        {
            return OrderType::MARKET;
        }
        throw std::runtime_error("orderType must be LIMIT or MARKET");
    }

    void validatePositive(uint64_t value, const char* fieldName)
    {
        if (value == 0)
        {
            throw std::runtime_error(std::string(fieldName) + " must be > 0");
        }
    }

    KafkaOrderCommandMessage parseOrderCommand(const std::string& body,
                                               const std::string& eventType)
    {
        const nlohmann::json json = nlohmann::json::parse(body);
        const uint64_t orderId = json.at("orderId").get<uint64_t>();
        const uint32_t instrumentId = json.at("instrumentId").get<uint32_t>();
        const uint32_t quantity = json.at("quantity").get<uint32_t>();
        const OrderType orderType = parseOrderType(json.value("orderType", "LIMIT"));
        const double price = json.value("price", 0.0);

        validatePositive(orderId, "orderId");
        validatePositive(instrumentId, "instrumentId");
        validatePositive(quantity, "quantity");

        if (orderType == OrderType::LIMIT && (!std::isfinite(price) || price <= 0))
        {
            throw std::runtime_error("LIMIT price must be finite and > 0");
        }

        return KafkaOrderCommandMessage{
            eventType,
            orderId,
            instrumentId,
            parseSide(json.at("side").get<std::string>()),
            orderType,
            price,
            quantity,
            json.value("timestamp", TimeUtils::getCurrentTime())
        };
    }

    KafkaOrderCommandMessage parseCancelCommand(const std::string& body)
    {
        const nlohmann::json json = nlohmann::json::parse(body);
        const uint64_t orderId = json.at("orderId").get<uint64_t>();
        const uint32_t instrumentId = json.at("instrumentId").get<uint32_t>();

        validatePositive(orderId, "orderId");
        validatePositive(instrumentId, "instrumentId");

        return KafkaOrderCommandMessage{
            KafkaEventTypes::ORDER_CANCEL,
            orderId,
            instrumentId,
            Side::BUY,
            OrderType::LIMIT,
            0.0,
            0,
            json.value("timestamp", TimeUtils::getCurrentTime())
        };
    }
}

BrokerApiHandler::BrokerApiHandler(IOrderCommandPublisher& publisher)
    : publisher(publisher)
{
}

BrokerApiResponse BrokerApiHandler::handleRequest(const std::string& method,
                                                  const std::string& path,
                                                  const std::string& body)
{
    ++metrics.totalRequests;

    if (method == "GET" && path == "/health")
    {
        return jsonResponse(200, {{"status", "ok"}});
    }

    if (method == "GET" && path == "/metrics")
    {
        return handleMetrics();
    }

    if (method == "POST" && path == "/orders")
    {
        return handlePlaceOrder(body);
    }

    if ((method == "PUT" && path == "/orders") ||
        (method == "POST" && path == "/orders/modify"))
    {
        return handleModifyOrder(body);
    }

    if ((method == "DELETE" && path == "/orders") ||
        (method == "POST" && path == "/orders/cancel"))
    {
        return handleCancelOrder(body);
    }

    ++metrics.rejectedRequests;
    return jsonResponse(404, {
        {"status", "error"},
        {"message", "unsupported endpoint"}
    });
}

BrokerApiResponse BrokerApiHandler::handlePlaceOrder(const std::string& body)
{
    try
    {
        return publishCommand(parseOrderCommand(body, KafkaEventTypes::ORDER_PLACE));
    }
    catch (const std::exception& ex)
    {
        ++metrics.rejectedRequests;
        return jsonResponse(400, {{"status", "error"}, {"message", ex.what()}});
    }
}

BrokerApiResponse BrokerApiHandler::handleModifyOrder(const std::string& body)
{
    try
    {
        return publishCommand(parseOrderCommand(body, KafkaEventTypes::ORDER_MODIFY));
    }
    catch (const std::exception& ex)
    {
        ++metrics.rejectedRequests;
        return jsonResponse(400, {{"status", "error"}, {"message", ex.what()}});
    }
}

BrokerApiResponse BrokerApiHandler::handleCancelOrder(const std::string& body)
{
    try
    {
        return publishCommand(parseCancelCommand(body));
    }
    catch (const std::exception& ex)
    {
        ++metrics.rejectedRequests;
        return jsonResponse(400, {{"status", "error"}, {"message", ex.what()}});
    }
}

BrokerApiMetrics BrokerApiHandler::getMetrics() const
{
    return metrics;
}

BrokerApiResponse BrokerApiHandler::handleMetrics() const
{
    return jsonResponse(200, {
        {"totalRequests", metrics.totalRequests},
        {"acceptedCommands", metrics.acceptedCommands},
        {"rejectedRequests", metrics.rejectedRequests},
        {"publishFailures", metrics.publishFailures}
    });
}

BrokerApiResponse BrokerApiHandler::publishCommand(const KafkaOrderCommandMessage& message)
{
    try
    {
        publisher.publishOrderCommand(message);
    }
    catch (const std::exception& ex)
    {
        ++metrics.publishFailures;
        return jsonResponse(503, {{"status", "error"}, {"message", ex.what()}});
    }

    ++metrics.acceptedCommands;

    return jsonResponse(202, {
        {"status", "accepted"},
        {"eventType", message.eventType},
        {"orderId", message.orderId},
        {"instrumentId", message.instrumentId}
    });
}
