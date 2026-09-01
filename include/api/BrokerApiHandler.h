#pragma once

#include <cstdint>
#include <string>

#include "api/IOrderCommandPublisher.h"

struct BrokerApiMetrics
{
    uint64_t totalRequests = 0;
    uint64_t acceptedCommands = 0;
    uint64_t rejectedRequests = 0;
    uint64_t publishFailures = 0;
};

struct BrokerApiResponse
{
    int statusCode;
    std::string body;
    std::string contentType = "application/json";
};

class BrokerApiHandler {
public:
    explicit BrokerApiHandler(IOrderCommandPublisher& publisher);

    BrokerApiResponse handleRequest(const std::string& method,
                                    const std::string& path,
                                    const std::string& body);
    BrokerApiMetrics getMetrics() const;

private:
    IOrderCommandPublisher& publisher;
    BrokerApiMetrics metrics;

    BrokerApiResponse handlePlaceOrder(const std::string& body);
    BrokerApiResponse handleModifyOrder(const std::string& body);
    BrokerApiResponse handleCancelOrder(const std::string& body);
    BrokerApiResponse handleMetrics() const;

    BrokerApiResponse publishCommand(const KafkaOrderCommandMessage& message);
};
