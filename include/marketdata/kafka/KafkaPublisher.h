#pragma once

#include <optional>
#include <string>

#include "core/Trade.h"
#include "marketdata/IMarketDataPublisher.h"

// Forward declarations
struct rd_kafka_s;
typedef struct rd_kafka_s rd_kafka_t;

struct rd_kafka_topic_s;
typedef struct rd_kafka_topic_s rd_kafka_topic_t;

// ------------------------------------------------------------
// KafkaPublisher
// - Publishes trade events to Kafka
// - Used for persistence, analytics, replay
// ------------------------------------------------------------
class KafkaPublisher : public IMarketDataPublisher {
public:
    KafkaPublisher(const std::string& brokerList,
                   const std::string& topic);

    ~KafkaPublisher();

    KafkaPublisher(const KafkaPublisher&) = delete;
    KafkaPublisher& operator=(const KafkaPublisher&) = delete;

    void publishTrade(const Trade& trade) override;
    void publishBookUpdate(uint32_t instrumentId,
                           std::optional<Price> bestBid,
                           std::optional<Price> bestAsk,
                           uint64_t timestamp) override;

private:
    std::string brokers;
    std::string topicName;

    rd_kafka_t* producer;
    rd_kafka_topic_t* kafkaTopic;

    // ---------------- Helpers ----------------

    void initProducer();

    void serializeAndSend(const Trade& trade);
    void serializeAndSendBookUpdate(uint32_t instrumentId,
                                    std::optional<Price> bestBid,
                                    std::optional<Price> bestAsk,
                                    uint64_t timestamp);

    std::string serializeTrade(const Trade& trade) const;
    std::string serializeBookUpdate(uint32_t instrumentId,
                                    std::optional<Price> bestBid,
                                    std::optional<Price> bestAsk,
                                    uint64_t timestamp) const;

    void flush();
};