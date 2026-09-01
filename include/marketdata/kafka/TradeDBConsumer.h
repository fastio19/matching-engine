#pragma once

#include <atomic>
#include <memory>
#include <string>

// Kafka forward declarations
struct rd_kafka_s;
typedef struct rd_kafka_s rd_kafka_t;

class PostgreSQLTradeStore;

// ------------------------------------------------------------
// TradeDBConsumer
// - Consumes trade events from Kafka
// - Persists them into PostgreSQL
// ------------------------------------------------------------
class TradeDBConsumer {
public:
    TradeDBConsumer(const std::string& brokers,
                    const std::string& topic,
                    const std::string& consumerGroup,
                    const std::string& dbConnectionString);

    ~TradeDBConsumer();

    // non-copyable
    TradeDBConsumer(const TradeDBConsumer&) = delete;
    TradeDBConsumer& operator=(const TradeDBConsumer&) = delete;

    // Start consumer loop
    void start();

    // Stop consumer loop
    void stop();

private:
    std::string brokerList;
    std::string topicName;
    std::string groupId;

    std::string dbConnStr;

    rd_kafka_t* consumer;

    std::unique_ptr<PostgreSQLTradeStore> tradeStore;

    std::atomic<bool> running;

    // ---------------- Kafka ----------------

    void initConsumer();

    void subscribe();

    void consumeLoop();

    // ---------------- DB ----------------

    void initDatabase();

    void insertTrade(const std::string& payload);

    // ---------------- Processing ----------------

    bool processMessage(const std::string& payload);
};