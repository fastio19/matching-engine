#include "marketdata/kafka/KafkaOrderCommandConsumer.h"

#include <iostream>
#include <stdexcept>
#include <utility>

#include <librdkafka/rdkafka.h>

#include "marketdata/kafka/KafkaSchema.h"

KafkaOrderCommandConsumer::KafkaOrderCommandConsumer(const std::string& brokers,
                                                     const std::string& topic,
                                                     const std::string& consumerGroup)
    : brokerList(brokers),
      topicName(topic),
      groupId(consumerGroup),
      consumer(nullptr),
      running(false)
{
    initConsumer();
    subscribe();
}

KafkaOrderCommandConsumer::~KafkaOrderCommandConsumer()
{
    stop();

    if (consumer)
    {
        rd_kafka_consumer_close(consumer);
        rd_kafka_destroy(consumer);
    }
}

void KafkaOrderCommandConsumer::start()
{
    running = true;
    std::cout << "[KafkaOrderCommandConsumer] Started\n";
    consumeLoop();
}

void KafkaOrderCommandConsumer::stop()
{
    running = false;
}

void KafkaOrderCommandConsumer::setPlaceHandler(PlaceHandler handler)
{
    placeHandler = std::move(handler);
}

void KafkaOrderCommandConsumer::setModifyHandler(ModifyHandler handler)
{
    modifyHandler = std::move(handler);
}

void KafkaOrderCommandConsumer::setCancelHandler(CancelHandler handler)
{
    cancelHandler = std::move(handler);
}

void KafkaOrderCommandConsumer::setDlqHandler(DlqHandler handler)
{
    dlqHandler = std::move(handler);
}

void KafkaOrderCommandConsumer::initConsumer()
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
                          brokerList.c_str(),
                          errstr,
                          sizeof(errstr))
        != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    if (rd_kafka_conf_set(conf,
                          "group.id",
                          groupId.c_str(),
                          errstr,
                          sizeof(errstr))
        != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    if (rd_kafka_conf_set(conf,
                          "auto.offset.reset",
                          "earliest",
                          errstr,
                          sizeof(errstr))
        != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    consumer = rd_kafka_new(RD_KAFKA_CONSUMER,
                            conf,
                            errstr,
                            sizeof(errstr));

    if (!consumer)
    {
        throw std::runtime_error(errstr);
    }

    rd_kafka_poll_set_consumer(consumer);
}

void KafkaOrderCommandConsumer::subscribe()
{
    rd_kafka_topic_partition_list_t* topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics,
                                      topicName.c_str(),
                                      RD_KAFKA_PARTITION_UA);

    rd_kafka_resp_err_t err = rd_kafka_subscribe(consumer, topics);
    rd_kafka_topic_partition_list_destroy(topics);

    if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
        throw std::runtime_error(rd_kafka_err2str(err));
    }
}

void KafkaOrderCommandConsumer::consumeLoop()
{
    while (running)
    {
        rd_kafka_message_t* msg = rd_kafka_consumer_poll(consumer, 1000);
        if (!msg)
        {
            continue;
        }

        if (msg->err)
        {
            std::cerr << "[KafkaOrderCommandConsumer] Kafka error: "
                      << rd_kafka_message_errstr(msg) << "\n";
            rd_kafka_message_destroy(msg);
            continue;
        }

        std::string payload(static_cast<char*>(msg->payload), msg->len);

        try
        {
            processMessage(payload);
        }
        catch (const std::exception& ex)
        {
            sendToDlq(payload, ex.what());
        }

        rd_kafka_message_destroy(msg);
    }
}

void KafkaOrderCommandConsumer::processMessage(const std::string& payload)
{
    const KafkaOrderCommandMessage message = parseOrderCommandMessage(payload);
    dispatchOrderCommandMessage(message, KafkaOrderCommandHandlers{
        placeHandler,
        modifyHandler,
        cancelHandler
    });
}

void KafkaOrderCommandConsumer::sendToDlq(const std::string& payload, const std::string& reason)
{
    std::cerr << "[KafkaOrderCommandConsumer] DLQ: " << reason << "\n";
    if (dlqHandler)
    {
        dlqHandler(payload, reason);
    }
}
