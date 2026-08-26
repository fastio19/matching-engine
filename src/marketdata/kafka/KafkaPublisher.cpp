// KafkaPublisher.cpp

#include "marketdata/kafka/KafkaPublisher.h"

#include <sstream>
#include <stdexcept>

#include <librdkafka/rdkafka.h>

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
    std::ostringstream oss;

    oss << "{"
        << "\"eventType\":\"TRADE\","
        << "\"tradeId\":" << trade.tradeId << ","
        << "\"buyOrderId\":" << trade.buyOrderId << ","
        << "\"sellOrderId\":" << trade.sellOrderId << ","
        << "\"instrumentId\":" << trade.instrumentId << ","
        << "\"price\":" << trade.price << ","
        << "\"quantity\":" << trade.quantity << ","
        << "\"timestamp\":" << trade.timestamp
        << "}";

    return oss.str();
}

// ------------------------------------------------------------
// serializeAndSend
// ------------------------------------------------------------
void KafkaPublisher::serializeAndSend(const Trade& trade)
{
    std::string payload = serializeTrade(trade);
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
            nullptr,
            0,
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