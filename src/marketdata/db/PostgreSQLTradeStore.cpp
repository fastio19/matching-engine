#include "marketdata/db/PostgreSQLTradeStore.h"

#include <stdexcept>

#include <pqxx/pqxx>

PostgreSQLTradeStore::PostgreSQLTradeStore(const std::string& connectionString)
    : connection(std::make_unique<pqxx::connection>(connectionString))
{
    if (!connection->is_open())
    {
        throw std::runtime_error("Failed to connect to PostgreSQL");
    }

    ensureSchema();
}

PostgreSQLTradeStore::~PostgreSQLTradeStore() = default;

bool PostgreSQLTradeStore::isOpen() const
{
    return connection && connection->is_open();
}

void PostgreSQLTradeStore::ensureSchema()
{
    pqxx::work txn(*connection);
    txn.exec(
        R"(
            CREATE TABLE IF NOT EXISTS trades (
                trade_id BIGINT PRIMARY KEY,
                buy_order_id BIGINT NOT NULL,
                sell_order_id BIGINT NOT NULL,
                instrument_id INTEGER NOT NULL,
                price DOUBLE PRECISION NOT NULL,
                quantity INTEGER NOT NULL,
                timestamp BIGINT NOT NULL
            )
        )");
    txn.commit();
}

void PostgreSQLTradeStore::insertTrade(const KafkaTradeEventMessage& tradeMessage)
{
    if (!isOpen())
    {
        throw std::runtime_error("PostgreSQL connection is not open");
    }

    pqxx::work txn(*connection);
    txn.exec_params(
        R"(
            INSERT INTO trades (
                trade_id,
                buy_order_id,
                sell_order_id,
                instrument_id,
                price,
                quantity,
                timestamp
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7)
            ON CONFLICT (trade_id) DO NOTHING
        )",
        tradeMessage.tradeId,
        tradeMessage.buyOrderId,
        tradeMessage.sellOrderId,
        tradeMessage.instrumentId,
        tradeMessage.price,
        tradeMessage.quantity,
        tradeMessage.timestamp
    );
    txn.commit();
}

uint64_t PostgreSQLTradeStore::countTradeById(uint64_t tradeId)
{
    if (!isOpen())
    {
        throw std::runtime_error("PostgreSQL connection is not open");
    }

    pqxx::work txn(*connection);
    const pqxx::result result = txn.exec_params(
        "SELECT COUNT(*) FROM trades WHERE trade_id = $1",
        tradeId);
    txn.commit();
    return result.at(0).at(0).as<uint64_t>();
}

void PostgreSQLTradeStore::deleteTradeById(uint64_t tradeId)
{
    if (!isOpen())
    {
        throw std::runtime_error("PostgreSQL connection is not open");
    }

    pqxx::work txn(*connection);
    txn.exec_params("DELETE FROM trades WHERE trade_id = $1", tradeId);
    txn.commit();
}
