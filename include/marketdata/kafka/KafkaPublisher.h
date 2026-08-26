#pragma once

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

private:
    std::string brokers;
    std::string topicName;

    rd_kafka_t* producer;
    rd_kafka_topic_t* kafkaTopic;

    // ---------------- Helpers ----------------

    void initProducer();

    void serializeAndSend(const Trade& trade);

    std::string serializeTrade(const Trade& trade) const;

    void flush();
};