#include "marketdata/kafka/TradeDBConsumer.h"

#include <iostream>
#include <stdexcept>

#include <librdkafka/rdkafka.h>

#include "marketdata/db/PostgreSQLTradeStore.h"
#include "marketdata/kafka/KafkaSchema.h"

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
TradeDBConsumer::TradeDBConsumer(
    const std::string& brokers,
    const std::string& topic,
    const std::string& consumerGroup,
    const std::string& dbConnectionString)
    : brokerList(brokers),
      topicName(topic),
      groupId(consumerGroup),
      dbConnStr(dbConnectionString),
      consumer(nullptr),
      running(false)
{
    initConsumer();

    subscribe();

    initDatabase();
}

// ------------------------------------------------------------
// Destructor
// ------------------------------------------------------------
TradeDBConsumer::~TradeDBConsumer()
{
    stop();

    if (consumer)
    {
        rd_kafka_consumer_close(consumer);

        rd_kafka_destroy(consumer);
    }

}

// ------------------------------------------------------------
// start
// ------------------------------------------------------------
void TradeDBConsumer::start()
{
    running = true;

    std::cout << "[TradeDBConsumer] Started\n";

    consumeLoop();
}

// ------------------------------------------------------------
// stop
// ------------------------------------------------------------
void TradeDBConsumer::stop()
{
    running = false;
}

// ------------------------------------------------------------
// initConsumer
// ------------------------------------------------------------
void TradeDBConsumer::initConsumer()
{
    char errstr[512];

    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    auto throwConfigError = [&](const char* message)
    {
        rd_kafka_conf_destroy(conf);
        throw std::runtime_error(message);
    };

    // Brokers
    if (rd_kafka_conf_set(conf,
                          "bootstrap.servers",
                          brokerList.c_str(),
                          errstr,
                          sizeof(errstr))
        != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    // Consumer group
    if (rd_kafka_conf_set(conf,
                          "group.id",
                          groupId.c_str(),
                          errstr,
                          sizeof(errstr))
        != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    // Start from beginning
    if (rd_kafka_conf_set(conf,
                          "auto.offset.reset",
                          "earliest",
                          errstr,
                          sizeof(errstr))
        != RD_KAFKA_CONF_OK)
    {
        throwConfigError(errstr);
    }

    // Commit offsets only after successful DB persistence.
    if (rd_kafka_conf_set(conf,
                          "enable.auto.commit",
                          "false",
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

    std::cout << "[TradeDBConsumer] Kafka consumer initialized\n";
}

// ------------------------------------------------------------
// subscribe
// ------------------------------------------------------------
void TradeDBConsumer::subscribe()
{
    rd_kafka_topic_partition_list_t* topics =
        rd_kafka_topic_partition_list_new(1);

    rd_kafka_topic_partition_list_add(
        topics,
        topicName.c_str(),
        RD_KAFKA_PARTITION_UA
    );

    rd_kafka_resp_err_t err =
        rd_kafka_subscribe(consumer, topics);

    rd_kafka_topic_partition_list_destroy(topics);

    if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
        throw std::runtime_error(
            rd_kafka_err2str(err)
        );
    }

    std::cout << "[TradeDBConsumer] Subscribed to topic: "
              << topicName
              << "\n";
}

// ------------------------------------------------------------
// initDatabase
// ------------------------------------------------------------
void TradeDBConsumer::initDatabase()
{
    tradeStore = std::make_unique<PostgreSQLTradeStore>(dbConnStr);
    std::cout << "[TradeDBConsumer] Connected to PostgreSQL\n";
}

// ------------------------------------------------------------
// consumeLoop
// ------------------------------------------------------------
void TradeDBConsumer::consumeLoop()
{
    while (running)
    {
        rd_kafka_message_t* msg =
            rd_kafka_consumer_poll(consumer, 1000);

        if (!msg)
        {
            continue;
        }

        if (msg->err)
        {
            std::cerr << "[TradeDBConsumer] Kafka error: "
                      << rd_kafka_message_errstr(msg)
                      << "\n";
        }
        else
        {
            std::string payload(
                static_cast<char*>(msg->payload),
                msg->len
            );

            if (processMessage(payload))
            {
                rd_kafka_resp_err_t commitErr =
                    rd_kafka_commit_message(consumer, msg, 0);
                if (commitErr != RD_KAFKA_RESP_ERR_NO_ERROR)
                {
                    std::cerr << "[TradeDBConsumer] Kafka commit failed: "
                              << rd_kafka_err2str(commitErr)
                              << "\n";
                }
            }
        }

        rd_kafka_message_destroy(msg);
    }
}

// ------------------------------------------------------------
// processMessage
// ------------------------------------------------------------
bool TradeDBConsumer::processMessage(
    const std::string& payload)
{
    try
    {
        insertTrade(payload);

        std::cout << "[TradeDBConsumer] Trade persisted\n";
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[TradeDBConsumer] DB insert failed: "
                  << ex.what()
                  << "\n";
        return false;
    }
}

// ------------------------------------------------------------
// insertTrade
// ------------------------------------------------------------
void TradeDBConsumer::insertTrade(
    const std::string& payload)
{
    if (!tradeStore || !tradeStore->isOpen())
    {
        throw std::runtime_error("PostgreSQL connection is not open");
    }

    const KafkaTradeEventMessage tradeMessage = parseTradeEventMessage(payload);
    tradeStore->insertTrade(tradeMessage);
}