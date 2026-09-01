#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "core/Order.h"
#include "core/Trade.h"

namespace KafkaTopics
{
    inline constexpr const char* ORDER_COMMANDS = "orders.commands";
    inline constexpr const char* ORDER_TRADES = "orders.trades";
    inline constexpr const char* ORDER_BOOK = "orders.book";
    inline constexpr const char* ORDER_DLQ = "orders.dlq";
}

namespace KafkaEventTypes
{
    inline constexpr const char* ORDER_PLACE = "ORDER_PLACE";
    inline constexpr const char* ORDER_CANCEL = "ORDER_CANCEL";
    inline constexpr const char* ORDER_MODIFY = "ORDER_MODIFY";
    inline constexpr const char* TRADE = "TRADE";
    inline constexpr const char* LAST_TRADED_PRICE = "LAST_TRADED_PRICE";
    inline constexpr const char* BOOK_UPDATE = "BOOK_UPDATE";
}

struct KafkaOrderCommandMessage
{
    std::string eventType;
    uint64_t orderId;
    uint32_t instrumentId;
    Side side;
    OrderType orderType;
    double price;
    uint32_t quantity;
    uint64_t timestamp;

    static KafkaOrderCommandMessage fromOrder(const Order& order,
                                              const std::string& commandType)
    {
        return KafkaOrderCommandMessage{
            commandType,
            order.id,
            order.instrumentId,
            order.side,
            order.type,
            order.price,
            order.quantity,
            order.timestamp
        };
    }

    Order toOrder() const
    {
        return Order{
            orderId,
            price,
            quantity,
            side,
            orderType,
            Market::NSE,
            timestamp,
            instrumentId
        };
    }
};

struct KafkaTradeEventMessage
{
    std::string eventType;
    uint64_t tradeId;
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    uint32_t instrumentId;
    double price;
    uint32_t quantity;
    uint64_t timestamp;

    static KafkaTradeEventMessage fromTrade(const Trade& trade)
    {
        return KafkaTradeEventMessage{
            KafkaEventTypes::TRADE,
            trade.tradeId,
            trade.buyOrderId,
            trade.sellOrderId,
            trade.instrumentId,
            trade.price,
            trade.quantity,
            trade.timestamp
        };
    }
};

struct KafkaLastTradedPriceMessage
{
    std::string eventType;
    uint32_t instrumentId;
    double lastTradedPrice;
    uint64_t timestamp;

    static KafkaLastTradedPriceMessage fromLastTradedPrice(uint32_t instrumentId,
                                                           double lastTradedPrice,
                                                           uint64_t timestamp)
    {
        return KafkaLastTradedPriceMessage{
            KafkaEventTypes::LAST_TRADED_PRICE,
            instrumentId,
            lastTradedPrice,
            timestamp
        };
    }
};

struct KafkaBookUpdateMessage
{
    std::string eventType;
    uint32_t instrumentId;
    double bestBid;
    double bestAsk;
    uint64_t timestamp;
};

struct KafkaOrderCommandHandlers
{
    std::function<void(const Order&)> onPlace;
    std::function<void(const Order&)> onModify;
    std::function<void(OrderId, uint32_t)> onCancel;
};

inline nlohmann::json toJson(const KafkaOrderCommandMessage& message)
{
    return {
        {"eventType", message.eventType},
        {"orderId", message.orderId},
        {"instrumentId", message.instrumentId},
        {"side", message.side == Side::BUY ? "BUY" : "SELL"},
        {"orderType", message.orderType == OrderType::LIMIT ? "LIMIT" : "MARKET"},
        {"price", message.price},
        {"quantity", message.quantity},
        {"timestamp", message.timestamp}
    };
}

inline KafkaOrderCommandMessage parseOrderCommandMessage(const std::string& payload)
{
    nlohmann::json json = nlohmann::json::parse(payload);

    const std::string eventType = json.at("eventType").get<std::string>();
    if (eventType != KafkaEventTypes::ORDER_PLACE &&
        eventType != KafkaEventTypes::ORDER_CANCEL &&
        eventType != KafkaEventTypes::ORDER_MODIFY)
    {
        throw std::runtime_error("Unsupported order command type: " + eventType);
    }

    auto parseSide = [](const std::string& sideValue)
    {
        if (sideValue == "BUY")
        {
            return Side::BUY;
        }
        if (sideValue == "SELL")
        {
            return Side::SELL;
        }
        throw std::runtime_error("Invalid side: " + sideValue);
    };

    auto parseOrderType = [](const std::string& typeValue)
    {
        if (typeValue == "LIMIT")
        {
            return OrderType::LIMIT;
        }
        if (typeValue == "MARKET")
        {
            return OrderType::MARKET;
        }
        throw std::runtime_error("Invalid orderType: " + typeValue);
    };

    return KafkaOrderCommandMessage{
        eventType,
        json.at("orderId").get<uint64_t>(),
        json.at("instrumentId").get<uint32_t>(),
        parseSide(json.value("side", "BUY")),
        parseOrderType(json.value("orderType", "LIMIT")),
        json.value("price", 0.0),
        json.value("quantity", 0u),
        json.at("timestamp").get<uint64_t>()
    };
}

inline nlohmann::json toJson(const KafkaTradeEventMessage& message)
{
    return {
        {"eventType", message.eventType},
        {"tradeId", message.tradeId},
        {"buyOrderId", message.buyOrderId},
        {"sellOrderId", message.sellOrderId},
        {"instrumentId", message.instrumentId},
        {"price", message.price},
        {"quantity", message.quantity},
        {"timestamp", message.timestamp}
    };
}

inline nlohmann::json toJson(const KafkaLastTradedPriceMessage& message)
{
    return {
        {"eventType", message.eventType},
        {"instrumentId", message.instrumentId},
        {"lastTradedPrice", message.lastTradedPrice},
        {"timestamp", message.timestamp}
    };
}

inline nlohmann::json toJson(const KafkaBookUpdateMessage& message)
{
    return {
        {"eventType", message.eventType},
        {"instrumentId", message.instrumentId},
        {"bestBid", message.bestBid},
        {"bestAsk", message.bestAsk},
        {"timestamp", message.timestamp}
    };
}

inline KafkaTradeEventMessage parseTradeEventMessage(const std::string& payload)
{
    nlohmann::json json = nlohmann::json::parse(payload);

    return KafkaTradeEventMessage{
        json.at("eventType").get<std::string>(),
        json.at("tradeId").get<uint64_t>(),
        json.at("buyOrderId").get<uint64_t>(),
        json.at("sellOrderId").get<uint64_t>(),
        json.at("instrumentId").get<uint32_t>(),
        json.at("price").get<double>(),
        json.at("quantity").get<uint32_t>(),
        json.at("timestamp").get<uint64_t>()
    };
}

inline KafkaLastTradedPriceMessage parseLastTradedPriceMessage(const std::string& payload)
{
    nlohmann::json json = nlohmann::json::parse(payload);

    const std::string eventType = json.at("eventType").get<std::string>();
    if (eventType != KafkaEventTypes::LAST_TRADED_PRICE)
    {
        throw std::runtime_error("Unsupported LTP type: " + eventType);
    }

    return KafkaLastTradedPriceMessage{
        eventType,
        json.at("instrumentId").get<uint32_t>(),
        json.at("lastTradedPrice").get<double>(),
        json.at("timestamp").get<uint64_t>()
    };
}

inline KafkaBookUpdateMessage parseBookUpdateMessage(const std::string& payload)
{
    nlohmann::json json = nlohmann::json::parse(payload);

    const std::string eventType = json.at("eventType").get<std::string>();
    if (eventType != KafkaEventTypes::BOOK_UPDATE)
    {
        throw std::runtime_error("Unsupported book update type: " + eventType);
    }

    return KafkaBookUpdateMessage{
        eventType,
        json.at("instrumentId").get<uint32_t>(),
        json.value("bestBid", 0.0),
        json.value("bestAsk", 0.0),
        json.at("timestamp").get<uint64_t>()
    };
}

inline void dispatchOrderCommandMessage(const KafkaOrderCommandMessage& message,
                                        const KafkaOrderCommandHandlers& handlers)
{
    if (message.eventType == KafkaEventTypes::ORDER_PLACE)
    {
        if (!handlers.onPlace)
        {
            throw std::runtime_error("Place handler not configured");
        }
        handlers.onPlace(message.toOrder());
        return;
    }

    if (message.eventType == KafkaEventTypes::ORDER_MODIFY)
    {
        if (!handlers.onModify)
        {
            throw std::runtime_error("Modify handler not configured");
        }
        handlers.onModify(message.toOrder());
        return;
    }

    if (message.eventType == KafkaEventTypes::ORDER_CANCEL)
    {
        if (!handlers.onCancel)
        {
            throw std::runtime_error("Cancel handler not configured");
        }
        handlers.onCancel(message.orderId, message.instrumentId);
        return;
    }

    throw std::runtime_error("Unsupported command event type: " + message.eventType);
}
