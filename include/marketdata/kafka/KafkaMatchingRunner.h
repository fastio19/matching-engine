#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "marketdata/kafka/KafkaSchema.h"
#include "marketdata/kafka/KafkaOrderCommandConsumer.h"
#include "marketdata/kafka/KafkaPublisher.h"
#include "marketdata/udp/UDPPublisher.h"
#include "orderbook/MatchingEngine.h"
#include "recovery/FileOrderStateStore.h"

class KafkaMatchingRunner {
public:
    KafkaMatchingRunner(const std::string& brokers,
                        const std::string& commandTopic,
                        const std::string& consumerGroup,
                        const std::string& tradeTopic = KafkaTopics::ORDER_TRADES,
                        const std::string& bookTopic = KafkaTopics::ORDER_BOOK,
                        const std::string& udpMulticastGroup = "239.0.0.1",
                        uint16_t udpMulticastPort = 5000,
                        const std::string& recoverySnapshotPath = "matching-engine-state.json");
    ~KafkaMatchingRunner();

    KafkaMatchingRunner(const KafkaMatchingRunner&) = delete;
    KafkaMatchingRunner& operator=(const KafkaMatchingRunner&) = delete;

    void start();
    void stop();

    MatchingEngine& engine();
    const MatchingEngine& engine() const;

private:
    MatchingEngine matchingEngine;
    KafkaOrderCommandConsumer commandConsumer;
    std::unique_ptr<KafkaPublisher> tradePublisher;
    std::unique_ptr<KafkaPublisher> bookPublisher;
    std::unique_ptr<UDPPublisher> udpPublisher;
    FileOrderStateStore stateStore;
    std::thread consumerThread;
    std::atomic<bool> running;

    void wireHandlers();
    void saveStateSnapshot() const;
};
