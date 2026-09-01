#include <exception>
#include <iostream>
#include <string>

#include "marketdata/db/PostgreSQLTradeStore.h"
#include "marketdata/kafka/KafkaSchema.h"
#include "utils/TimeUtils.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: trade_db_smoke_test \"host=localhost port=5432 dbname=matching_engine user=postgres password=secret\" [trade_id] [--keep-row]\n";
        return 2;
    }

    const std::string connectionString = argv[1];
    const uint64_t tradeId = (argc > 2 && std::string(argv[2]) != "--keep-row")
        ? std::stoull(argv[2])
        : 9000000000001ULL;
    const bool keepRow = (argc > 2 && std::string(argv[2]) == "--keep-row") ||
                         (argc > 3 && std::string(argv[3]) == "--keep-row");

    try
    {
        PostgreSQLTradeStore store(connectionString);

        const KafkaTradeEventMessage trade{
            KafkaEventTypes::TRADE,
            tradeId,
            7001,
            8001,
            101,
            105.25,
            50,
            TimeUtils::getCurrentTime()
        };

        store.deleteTradeById(tradeId);
        store.insertTrade(trade);
        store.insertTrade(trade);

        const uint64_t rowCount = store.countTradeById(tradeId);
        if (rowCount != 1)
        {
            std::cerr << "DB smoke failed: expected exactly one idempotent trade row, found "
                      << rowCount << "\n";
            return 1;
        }

        if (!keepRow)
        {
            store.deleteTradeById(tradeId);
        }

        std::cout << "DB smoke passed: connected, ensured schema, inserted trade id "
                  << tradeId
                  << ", verified idempotency";
        if (!keepRow)
        {
            std::cout << ", cleaned up test row";
        }
        std::cout << "\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "DB smoke failed: " << ex.what() << "\n";
        return 1;
    }
}
