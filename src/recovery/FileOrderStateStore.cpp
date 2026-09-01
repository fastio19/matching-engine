#include "recovery/FileOrderStateStore.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace
{
    std::string sideToString(Side side)
    {
        return side == Side::BUY ? "BUY" : "SELL";
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
        throw std::runtime_error("Invalid snapshot side: " + value);
    }

    std::string orderTypeToString(OrderType orderType)
    {
        return orderType == OrderType::LIMIT ? "LIMIT" : "MARKET";
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
        throw std::runtime_error("Invalid snapshot orderType: " + value);
    }

    std::string marketToString(Market market)
    {
        return market == Market::NSE ? "NSE" : "BSE";
    }

    Market parseMarket(const std::string& value)
    {
        if (value == "NSE")
        {
            return Market::NSE;
        }
        if (value == "BSE")
        {
            return Market::BSE;
        }
        throw std::runtime_error("Invalid snapshot market: " + value);
    }

    nlohmann::json orderToJson(const Order& order)
    {
        return {
            {"orderId", order.id},
            {"price", order.price},
            {"quantity", order.quantity},
            {"side", sideToString(order.side)},
            {"orderType", orderTypeToString(order.type)},
            {"market", marketToString(order.market)},
            {"timestamp", order.timestamp},
            {"instrumentId", order.instrumentId}
        };
    }

    Order jsonToOrder(const nlohmann::json& json)
    {
        return Order{
            json.at("orderId").get<OrderId>(),
            json.at("price").get<Price>(),
            json.at("quantity").get<uint32_t>(),
            parseSide(json.at("side").get<std::string>()),
            parseOrderType(json.at("orderType").get<std::string>()),
            parseMarket(json.at("market").get<std::string>()),
            json.at("timestamp").get<uint64_t>(),
            json.at("instrumentId").get<uint32_t>()
        };
    }
}

FileOrderStateStore::FileOrderStateStore(std::string snapshotPath)
    : snapshotPath(std::move(snapshotPath))
{
}

std::vector<Order> FileOrderStateStore::loadOpenOrders() const
{
    std::ifstream input(snapshotPath);
    if (!input.good())
    {
        return {};
    }

    nlohmann::json snapshot = nlohmann::json::parse(input);
    std::vector<Order> orders;
    for (const auto& item : snapshot.at("openOrders"))
    {
        orders.push_back(jsonToOrder(item));
    }

    return orders;
}

void FileOrderStateStore::saveOpenOrders(const std::vector<Order>& orders) const
{
    nlohmann::json snapshot;
    snapshot["openOrders"] = nlohmann::json::array();

    for (const Order& order : orders)
    {
        snapshot["openOrders"].push_back(orderToJson(order));
    }

    const std::string tempPath = snapshotPath + ".tmp";
    {
        std::ofstream output(tempPath, std::ios::trunc);
        if (!output.is_open())
        {
            throw std::runtime_error("Failed to open recovery snapshot for writing: " + tempPath);
        }

        output << snapshot.dump(2);
        output.flush();
        if (!output.good())
        {
            throw std::runtime_error("Failed to write recovery snapshot: " + tempPath);
        }
    }

    std::remove(snapshotPath.c_str());
    if (std::rename(tempPath.c_str(), snapshotPath.c_str()) != 0)
    {
        std::remove(tempPath.c_str());
        throw std::runtime_error("Failed to replace recovery snapshot: " + snapshotPath);
    }
}
