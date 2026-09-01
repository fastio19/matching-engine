#include <cstdlib>
#include <iostream>
#include <string>

#include "marketdata/kafka/KafkaSchema.h"
#include "marketdata/kafka/TradeDBConsumer.h"

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
    const std::string brokers = getConfigValue(argc, argv, 1, "TRADE_DB_KAFKA_BROKERS", "localhost:9092");
    const std::string topic = getConfigValue(argc, argv, 2, "TRADE_DB_TRADE_TOPIC", KafkaTopics::ORDER_TRADES);
    const std::string groupId = getConfigValue(argc, argv, 3, "TRADE_DB_CONSUMER_GROUP", "trade-db-consumer-group");
    const std::string dbConnectionString = getConfigValue(
        argc,
        argv,
        4,
        "TRADE_DB_CONNECTION_STRING",
        "host=localhost port=5432 dbname=matching_engine user=postgres password=postgres");

    std::cout << "Trade DB consumer starting. Kafka brokers=" << brokers
              << ", topic=" << topic
              << ", group=" << groupId
              << "\n";

    TradeDBConsumer consumer(brokers, topic, groupId, dbConnectionString);
    consumer.start();

    return 0;
}
