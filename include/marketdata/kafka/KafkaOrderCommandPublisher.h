#pragma once

#include <string>

#include "api/IOrderCommandPublisher.h"

struct rd_kafka_s;
typedef struct rd_kafka_s rd_kafka_t;

struct rd_kafka_topic_s;
typedef struct rd_kafka_topic_s rd_kafka_topic_t;

class KafkaOrderCommandPublisher : public IOrderCommandPublisher {
public:
    KafkaOrderCommandPublisher(const std::string& brokerList,
                               const std::string& topic);
    ~KafkaOrderCommandPublisher();

    KafkaOrderCommandPublisher(const KafkaOrderCommandPublisher&) = delete;
    KafkaOrderCommandPublisher& operator=(const KafkaOrderCommandPublisher&) = delete;

    void publishOrderCommand(const KafkaOrderCommandMessage& message) override;

private:
    std::string brokers;
    std::string topicName;
    rd_kafka_t* producer;
    rd_kafka_topic_t* kafkaTopic;

    void initProducer();
    void flush();
};
