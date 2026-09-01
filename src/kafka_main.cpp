#include <iostream>
#include <cstdlib>
#include <string>

#include "marketdata/kafka/KafkaMatchingRunner.h"
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
    const std::string brokers = getConfigValue(argc, argv, 1, "MATCHING_KAFKA_BROKERS", "localhost:9092");
    const std::string topic = getConfigValue(argc, argv, 2, "MATCHING_COMMAND_TOPIC", KafkaTopics::ORDER_COMMANDS);
    const std::string groupId = getConfigValue(argc, argv, 3, "MATCHING_CONSUMER_GROUP", "matching-engine-group");
    const std::string tradeTopic = getConfigValue(argc, argv, 4, "MATCHING_TRADE_TOPIC", KafkaTopics::ORDER_TRADES);
    const std::string bookTopic = getConfigValue(argc, argv, 5, "MATCHING_BOOK_TOPIC", KafkaTopics::ORDER_BOOK);
    const std::string udpGroup = getConfigValue(argc, argv, 6, "MATCHING_UDP_GROUP", "239.0.0.1");
    const uint16_t udpPort = static_cast<uint16_t>(
        std::stoi(getConfigValue(argc, argv, 7, "MATCHING_UDP_PORT", "5000")));
    const std::string recoverySnapshotPath = getConfigValue(
        argc,
        argv,
        8,
        "MATCHING_RECOVERY_SNAPSHOT",
        "matching-engine-state.json");

    KafkaMatchingRunner runner(brokers, topic, groupId, tradeTopic, bookTopic, udpGroup, udpPort, recoverySnapshotPath);
    runner.start();

    std::cout << "Kafka matching runner started. Press Enter to stop...\n";
    std::cin.get();

    runner.stop();
    return 0;
}
