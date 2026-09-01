#include "marketdata/kafka/KafkaOrderCommandPublisher.h"

#include <stdexcept>

#include <librdkafka/rdkafka.h>

KafkaOrderCommandPublisher::KafkaOrderCommandPublisher(const std::string& brokerList,
                                                       const std::string& topic)
    : brokers(brokerList),
      topicName(topic),
      producer(nullptr),
      kafkaTopic(nullptr)
{
    initProducer();
}

KafkaOrderCommandPublisher::~KafkaOrderCommandPublisher()
{
    flush();

    if (kafkaTopic)
    {
        rd_kafka_topic_destroy(kafkaTopic);
    }

    if (producer)
    {
        rd_kafka_destroy(producer);
    }
}

void KafkaOrderCommandPublisher::publishOrderCommand(const KafkaOrderCommandMessage& message)
{
    const std::string payload = toJson(message).dump();
    const std::string key = std::to_string(message.instrumentId);
    constexpr int maxQueueFullRetries = 3;
    int attempt = 0;

    while (true)
    {
        const int result = rd_kafka_produce(
            kafkaTopic,
            RD_KAFKA_PARTITION_UA,
            RD_KAFKA_MSG_F_COPY,
            const_cast<char*>(payload.c_str()),
            payload.size(),
            const_cast<char*>(key.c_str()),
            key.size(),
            nullptr
        );

        if (result == 0)
        {
            break;
        }

        const rd_kafka_resp_err_t err = rd_kafka_last_error();
        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL && attempt < maxQueueFullRetries)
        {
            ++attempt;
            rd_kafka_poll(producer, 100);
            continue;
        }

        throw std::runtime_error(
            std::string("Kafka command publish failed: ") +
            rd_kafka_err2str(err));
    }

    rd_kafka_poll(producer, 0);
}

void KafkaOrderCommandPublisher::initProducer()
{
    char errstr[512];
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    auto throwConfigError = [&](const char* message)
    {
        rd_kafka_conf_destroy(conf);
        throw std::runtime_error(message);
    };

    if (rd_kafka_conf_set(conf,
                          "bootstrap.servers",
                          brokers.c_str(),
                          errstr,
                          sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!producer)
    {
        throw std::runtime_error(errstr);
    }

    kafkaTopic = rd_kafka_topic_new(producer, topicName.c_str(), nullptr);
    if (!kafkaTopic)
    {
        rd_kafka_destroy(producer);
        producer = nullptr;
        throw std::runtime_error("Failed to create Kafka command topic");
    }
}

void KafkaOrderCommandPublisher::flush()
{
    if (producer)
    {
        rd_kafka_flush(producer, 5000);
    }
}
