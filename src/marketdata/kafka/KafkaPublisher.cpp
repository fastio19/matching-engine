// KafkaPublisher.cpp

#include "marketdata/kafka/KafkaPublisher.h"

#include <stdexcept>

#include <librdkafka/rdkafka.h>

#include "marketdata/kafka/KafkaSchema.h"

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
KafkaPublisher::KafkaPublisher(const std::string& brokerList,
                               const std::string& topic)
    : brokers(brokerList),
      topicName(topic),
      producer(nullptr),
      kafkaTopic(nullptr)
{
    initProducer();
}

// ------------------------------------------------------------
// Destructor
// ------------------------------------------------------------
KafkaPublisher::~KafkaPublisher()
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

// ------------------------------------------------------------
// publishTrade
// ------------------------------------------------------------
void KafkaPublisher::publishTrade(const Trade& trade)
{
    serializeAndSend(trade);
}

void KafkaPublisher::publishBookUpdate(uint32_t instrumentId,
                                       std::optional<Price> bestBid,
                                       std::optional<Price> bestAsk,
                                       uint64_t timestamp)
{
    serializeAndSendBookUpdate(instrumentId, bestBid, bestAsk, timestamp);
}

// ------------------------------------------------------------
// initProducer
// ------------------------------------------------------------
void KafkaPublisher::initProducer()
{
    char errstr[512];

    // Create Kafka config
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    auto throwConfigError = [&](const char* message)
    {
        rd_kafka_conf_destroy(conf);
        throw std::runtime_error(message);
    };

    // Set broker list
    if (rd_kafka_conf_set(conf,
                          "bootstrap.servers",
                          brokers.c_str(),
                          errstr,
                          sizeof(errstr))
        != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    // Create producer
    producer = rd_kafka_new(RD_KAFKA_PRODUCER,
                            conf,
                            errstr,
                            sizeof(errstr));

    if (!producer)
    {
        throw std::runtime_error(errstr);
    }

    // Create topic handle
    kafkaTopic = rd_kafka_topic_new(producer,
                                    topicName.c_str(),
                                    nullptr);

    if (!kafkaTopic)
    {
        rd_kafka_destroy(producer);
        producer = nullptr;
        throw std::runtime_error("Failed to create Kafka topic");
    }
}

// ------------------------------------------------------------
// serializeTrade
// ------------------------------------------------------------
std::string KafkaPublisher::serializeTrade(
    const Trade& trade) const
{
    return toJson(KafkaTradeEventMessage::fromTrade(trade)).dump();
}

std::string KafkaPublisher::serializeBookUpdate(uint32_t instrumentId,
                                                std::optional<Price> bestBid,
                                                std::optional<Price> bestAsk,
                                                uint64_t timestamp) const
{
    return toJson(KafkaBookUpdateMessage{
        KafkaEventTypes::BOOK_UPDATE,
        instrumentId,
        bestBid.value_or(0.0),
        bestAsk.value_or(0.0),
        timestamp
    }).dump();
}

// ------------------------------------------------------------
// serializeAndSend
// ------------------------------------------------------------
void KafkaPublisher::serializeAndSend(const Trade& trade)
{
    std::string payload = serializeTrade(trade);
    std::string key = std::to_string(trade.instrumentId);
    constexpr int maxQueueFullRetries = 3;
    int attempt = 0;

    while (true)
    {
        int result = rd_kafka_produce(
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

        rd_kafka_resp_err_t err = rd_kafka_last_error();
        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL &&
            attempt < maxQueueFullRetries)
        {
            ++attempt;
            rd_kafka_poll(producer, 100);
            continue;
        }

        throw std::runtime_error(
            std::string("Kafka publish failed: ") +
            rd_kafka_err2str(err));
    }

    // Serve delivery reports
    rd_kafka_poll(producer, 0);
}

void KafkaPublisher::serializeAndSendBookUpdate(uint32_t instrumentId,
                                                std::optional<Price> bestBid,
                                                std::optional<Price> bestAsk,
                                                uint64_t timestamp)
{
    std::string payload = serializeBookUpdate(instrumentId, bestBid, bestAsk, timestamp);
    std::string key = std::to_string(instrumentId);
    constexpr int maxQueueFullRetries = 3;
    int attempt = 0;

    while (true)
    {
        int result = rd_kafka_produce(
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

        rd_kafka_resp_err_t err = rd_kafka_last_error();
        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL &&
            attempt < maxQueueFullRetries)
        {
            ++attempt;
            rd_kafka_poll(producer, 100);
            continue;
        }

        throw std::runtime_error(
            std::string("Kafka publish failed: ") +
            rd_kafka_err2str(err));
    }

    rd_kafka_poll(producer, 0);
}

// ------------------------------------------------------------
// flush
// ------------------------------------------------------------
void KafkaPublisher::flush()
{
    if (producer)
    {
        rd_kafka_flush(producer, 5000);
    }
}