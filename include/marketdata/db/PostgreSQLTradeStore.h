#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "marketdata/kafka/KafkaSchema.h"

namespace pqxx
{
    class connection;
}

class PostgreSQLTradeStore {
public:
    explicit PostgreSQLTradeStore(const std::string& connectionString);
    ~PostgreSQLTradeStore();

    PostgreSQLTradeStore(const PostgreSQLTradeStore&) = delete;
    PostgreSQLTradeStore& operator=(const PostgreSQLTradeStore&) = delete;

    bool isOpen() const;
    void ensureSchema();
    void insertTrade(const KafkaTradeEventMessage& tradeMessage);
    uint64_t countTradeById(uint64_t tradeId);
    void deleteTradeById(uint64_t tradeId);

private:
    std::unique_ptr<pqxx::connection> connection;
};
