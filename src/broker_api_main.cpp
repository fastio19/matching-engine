#include <iostream>
#include <cstdlib>
#include <string>

#include "api/BrokerApiHandler.h"
#include "api/SimpleHttpServer.h"
#include "marketdata/kafka/KafkaOrderCommandPublisher.h"
#include "marketdata/kafka/KafkaSchema.h"

namespace
{
    std::string getConfigValue(int argc,
                               char** argv,
                               int argIndex,
                               const char* envName,
                               const std::string& defaultValue)
    {
        if (argc > argIndex)
        {
            return argv[argIndex];
        }

        const char* envValue = std::getenv(envName);
        return envValue ? std::string(envValue) : defaultValue;
    }
}

int main(int argc, char** argv)
{
    const std::string brokers = getConfigValue(argc, argv, 1, "BROKER_API_KAFKA_BROKERS", "localhost:9092");
    const std::string topic = getConfigValue(argc, argv, 2, "BROKER_API_COMMAND_TOPIC", KafkaTopics::ORDER_COMMANDS);
    const std::string host = getConfigValue(argc, argv, 3, "BROKER_API_HOST", "0.0.0.0");
    const uint16_t port = static_cast<uint16_t>(
        std::stoi(getConfigValue(argc, argv, 4, "BROKER_API_PORT", "8080")));

    KafkaOrderCommandPublisher publisher(brokers, topic);
    BrokerApiHandler apiHandler(publisher);
    SimpleHttpServer server(host,
                            port,
                            [&](const std::string& method,
                                const std::string& path,
                                const std::string& body)
                            {
                                return apiHandler.handleRequest(method, path, body);
                            });

    std::cout << "Broker API listening on " << host << ":" << port
              << ", publishing to Kafka topic " << topic
              << " at " << brokers << "\n";

    server.run();
    return 0;
}
