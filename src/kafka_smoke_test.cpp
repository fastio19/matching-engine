#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <librdkafka/rdkafka.h>

#include "marketdata/kafka/KafkaMatchingRunner.h"
#include "marketdata/kafka/KafkaSchema.h"

namespace
{
void kafkaLogCallback(const rd_kafka_t*, int level, const char* fac, const char* buf)
{
    std::clog << "[librdkafka][" << level << "][" << fac << "] " << buf << '\n';
}

struct KafkaProducerHandle
{
    rd_kafka_t* handle{nullptr};

    ~KafkaProducerHandle()
    {
        if (handle)
        {
            rd_kafka_flush(handle, 5000);
            rd_kafka_destroy(handle);
        }
    }
};

struct KafkaConsumerHandle
{
    rd_kafka_t* handle{nullptr};

    ~KafkaConsumerHandle()
    {
        if (handle)
        {
            rd_kafka_consumer_close(handle);
            rd_kafka_destroy(handle);
        }
    }
};

struct KafkaTopicHandle
{
    rd_kafka_topic_t* handle{nullptr};

    ~KafkaTopicHandle()
    {
        if (handle)
        {
            rd_kafka_topic_destroy(handle);
        }
    }
};

std::string makeRunId()
{
    return std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

KafkaProducerHandle createProducer(const std::string& brokers, bool debug)
{
    char errstr[512];
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    rd_kafka_conf_set_log_cb(conf, kafkaLogCallback);

    if (rd_kafka_conf_set(conf,
                          "bootstrap.servers",
                          brokers.c_str(),
                          errstr,
                          sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        rd_kafka_conf_destroy(conf);
        throw std::runtime_error(errstr);
    }

    if (debug &&
        rd_kafka_conf_set(conf, "debug", "all", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        rd_kafka_conf_destroy(conf);
        throw std::runtime_error(errstr);
    }

    rd_kafka_t* producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!producer)
    {
        throw std::runtime_error(errstr);
    }

    return KafkaProducerHandle{producer};
}

KafkaConsumerHandle createConsumer(const std::string& brokers, const std::string& groupId, bool debug)
{
    char errstr[512];
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    rd_kafka_conf_set_log_cb(conf, kafkaLogCallback);

    auto fail = [&](const char* message)
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
        fail(errstr);
    }

    if (rd_kafka_conf_set(conf,
                          "group.id",
                          groupId.c_str(),
                          errstr,
                          sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        fail(errstr);
    }

    if (rd_kafka_conf_set(conf,
                          "auto.offset.reset",
                          "earliest",
                          errstr,
                          sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        fail(errstr);
    }

    if (rd_kafka_conf_set(conf,
                          "enable.auto.commit",
                          "false",
                          errstr,
                          sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        fail(errstr);
    }

    if (debug &&
        rd_kafka_conf_set(conf, "debug", "all", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        fail(errstr);
    }

    rd_kafka_t* consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!consumer)
    {
        throw std::runtime_error(errstr);
    }

    rd_kafka_poll_set_consumer(consumer);
    return KafkaConsumerHandle{consumer};
}

void subscribeToTopic(rd_kafka_t* consumer, const std::string& topic, bool debug)
{
    if (debug)
    {
        std::clog << "Subscribing to " << topic << '\n';
    }

    rd_kafka_topic_partition_list_t* topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, topic.c_str(), RD_KAFKA_PARTITION_UA);

    const rd_kafka_resp_err_t err = rd_kafka_subscribe(consumer, topics);
    rd_kafka_topic_partition_list_destroy(topics);
    if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
        throw std::runtime_error(rd_kafka_err2str(err));
    }
}

void sendOrderCommand(rd_kafka_t* producer,
                      rd_kafka_topic_t* topicHandle,
                      const KafkaOrderCommandMessage& command,
                      bool debug)
{
    const std::string payload = toJson(command).dump();
    const std::string key = std::to_string(command.instrumentId);

    if (debug)
    {
        std::cout << "Sending command: " << payload << '\n';
    }

    const int produceResult = rd_kafka_produce(
        topicHandle,
        RD_KAFKA_PARTITION_UA,
        RD_KAFKA_MSG_F_COPY,
        const_cast<char*>(payload.c_str()),
        payload.size(),
        key.c_str(),
        key.size(),
        nullptr
    );

    if (produceResult != 0)
    {
        throw std::runtime_error(std::string("Kafka publish failed: ") + rd_kafka_err2str(rd_kafka_last_error()));
    }

    rd_kafka_poll(producer, 0);
}

KafkaTradeEventMessage waitForTrade(rd_kafka_t* consumer,
                                    OrderId expectedBuyOrderId,
                                    OrderId expectedSellOrderId,
                                    uint32_t expectedInstrumentId,
                                    int timeoutMs,
                                    bool debug)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    int idlePolls = 0;
    while (std::chrono::steady_clock::now() < deadline)
    {
        rd_kafka_message_t* msg = rd_kafka_consumer_poll(consumer, 250);
        if (!msg)
        {
            if (debug && (++idlePolls % 4 == 0))
            {
                std::cout << "Still waiting for trade..." << '\n';
            }
            continue;
        }

        if (msg->err)
        {
            const std::string error = rd_kafka_message_errstr(msg);
            rd_kafka_message_destroy(msg);
            throw std::runtime_error(error);
        }

        const std::string payload(static_cast<char*>(msg->payload), msg->len);
        rd_kafka_message_destroy(msg);

        if (debug)
        {
            std::cout << "Received candidate trade payload: " << payload << '\n';
        }

        const KafkaTradeEventMessage trade = parseTradeEventMessage(payload);
        if (trade.buyOrderId == expectedBuyOrderId &&
            trade.sellOrderId == expectedSellOrderId &&
            trade.instrumentId == expectedInstrumentId)
        {
            return trade;
        }
    }

    throw std::runtime_error("Timed out waiting for Kafka trade event");
}
}

int main(int argc, char** argv)
{
    try
    {
        bool debug = false;
        std::vector<std::string> args;
        args.reserve(static_cast<size_t>(argc > 0 ? argc - 1 : 0));
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "--debug" || arg == "-d")
            {
                debug = true;
                continue;
            }

            args.push_back(std::move(arg));
        }

        const std::string brokers = !args.empty() ? args[0] : "localhost:9092";
        const std::string commandTopic = (args.size() > 1) ? args[1] : KafkaTopics::ORDER_COMMANDS;
        const std::string tradeTopic = (args.size() > 2) ? args[2] : KafkaTopics::ORDER_TRADES;
        const std::string bookTopic = (args.size() > 3) ? args[3] : KafkaTopics::ORDER_BOOK;
        const std::string groupId = (args.size() > 4)
            ? args[4]
            : std::string("matching-engine-smoke-") + makeRunId();
        const int timeoutMs = (args.size() > 5) ? std::stoi(args[5]) : 15000;

        std::cout << "Kafka smoke test starting\n"
                  << "  brokers: " << brokers << '\n'
                  << "  command topic: " << commandTopic << '\n'
                  << "  trade topic: " << tradeTopic << '\n'
                  << "  book topic: " << bookTopic << '\n'
                  << "  group id: " << groupId << '\n'
                  << "  timeout ms: " << timeoutMs << '\n'
                  << "  debug: " << (debug ? "on" : "off") << std::endl;

        std::cout << "Creating matcher, producer, and consumer..." << std::endl;
        KafkaMatchingRunner runner(brokers, commandTopic, groupId, tradeTopic, bookTopic);
        KafkaProducerHandle producer = createProducer(brokers, debug);
        KafkaConsumerHandle consumer = createConsumer(brokers, groupId + "-verify", debug);
        KafkaTopicHandle commandTopicHandle{rd_kafka_topic_new(producer.handle, commandTopic.c_str(), nullptr)};
        if (!commandTopicHandle.handle)
        {
            throw std::runtime_error("Failed to create Kafka command topic handle");
        }
        subscribeToTopic(consumer.handle, tradeTopic, debug);

        std::cout << "Starting matcher..." << std::endl;
        runner.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        const OrderId sellOrderId = static_cast<OrderId>(std::stoull(makeRunId()));
        const OrderId buyOrderId = sellOrderId + 1;
        const uint32_t instrumentId = 1;

        std::cout << "Publishing sell order " << sellOrderId << std::endl;
        sendOrderCommand(producer.handle,
                         commandTopicHandle.handle,
                         KafkaOrderCommandMessage{
                             KafkaEventTypes::ORDER_PLACE,
                             sellOrderId,
                             instrumentId,
                             Side::SELL,
                             OrderType::LIMIT,
                             101.0,
                             50,
                             1724670000000ULL
                         },
                         debug);

        std::cout << "Publishing buy order " << buyOrderId << std::endl;
        sendOrderCommand(producer.handle,
                         commandTopicHandle.handle,
                         KafkaOrderCommandMessage{
                             KafkaEventTypes::ORDER_PLACE,
                             buyOrderId,
                             instrumentId,
                             Side::BUY,
                             OrderType::LIMIT,
                             105.0,
                             50,
                             1724670000001ULL
                         },
                         debug);

        std::cout << "Waiting for matching trade on " << tradeTopic << "..." << std::endl;
        const KafkaTradeEventMessage trade = waitForTrade(consumer.handle,
                                                          buyOrderId,
                                                          sellOrderId,
                                                           instrumentId,
                                                          timeoutMs,
                                                          debug);

        std::cout << "Smoke test passed: tradeId=" << trade.tradeId
                  << " buyOrderId=" << trade.buyOrderId
                  << " sellOrderId=" << trade.sellOrderId
                  << " instrumentId=" << trade.instrumentId
                  << " price=" << trade.price
                  << " quantity=" << trade.quantity << '\n';
        runner.stop();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Kafka smoke test failed: " << ex.what() << '\n';
        return 1;
    }
}
