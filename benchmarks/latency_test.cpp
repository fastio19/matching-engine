#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "core/Order.h"
#include "orderbook/MatchingEngine.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    Order makeOrder(OrderId id,
                    Side side,
                    Price price,
                    uint32_t quantity,
                    uint32_t instrumentId,
                    uint64_t timestamp)
    {
        return Order{
            id,
            price,
            quantity,
            side,
            OrderType::LIMIT,
            Market::NSE,
            timestamp,
            instrumentId
        };
    }

    struct LatencyStats
    {
        double averageNs;
        uint64_t p50Ns;
        uint64_t p95Ns;
        uint64_t p99Ns;
        uint64_t maxNs;
        double throughputPerSecond;
    };

    uint64_t percentile(const std::vector<uint64_t>& sortedValues, double percentileValue)
    {
        const std::size_t index = static_cast<std::size_t>(
            (percentileValue / 100.0) * static_cast<double>(sortedValues.size() - 1));
        return sortedValues[index];
    }

    LatencyStats summarize(std::vector<uint64_t> latenciesNs, double elapsedSeconds)
    {
        std::sort(latenciesNs.begin(), latenciesNs.end());
        const uint64_t totalNs = std::accumulate(latenciesNs.begin(), latenciesNs.end(), uint64_t{0});

        return LatencyStats{
            static_cast<double>(totalNs) / static_cast<double>(latenciesNs.size()),
            percentile(latenciesNs, 50),
            percentile(latenciesNs, 95),
            percentile(latenciesNs, 99),
            latenciesNs.back(),
            static_cast<double>(latenciesNs.size()) / elapsedSeconds
        };
    }

    void printStats(const std::string& scenario, const LatencyStats& stats)
    {
        std::cout << std::left << std::setw(28) << scenario
                  << " avg=" << std::fixed << std::setprecision(1) << stats.averageNs << "ns"
                  << " p50=" << stats.p50Ns << "ns"
                  << " p95=" << stats.p95Ns << "ns"
                  << " p99=" << stats.p99Ns << "ns"
                  << " max=" << stats.maxNs << "ns"
                  << " throughput=" << std::setprecision(0) << stats.throughputPerSecond << " orders/sec\n";
    }

    LatencyStats runRestingOrderBenchmark(std::size_t orderCount)
    {
        MatchingEngine engine;
        std::vector<uint64_t> latenciesNs;
        latenciesNs.reserve(orderCount);

        const auto benchmarkStart = Clock::now();
        for (std::size_t i = 0; i < orderCount; ++i)
        {
            const auto start = Clock::now();
            engine.processOrder(makeOrder(static_cast<OrderId>(i + 1),
                                          Side::BUY,
                                          100.0 - static_cast<double>(i % 100) * 0.01,
                                          100,
                                          1,
                                          static_cast<uint64_t>(i + 1)));
            const auto end = Clock::now();
            latenciesNs.push_back(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
        }
        const auto benchmarkEnd = Clock::now();

        const double elapsedSeconds = std::chrono::duration<double>(benchmarkEnd - benchmarkStart).count();
        return summarize(std::move(latenciesNs), elapsedSeconds);
    }

    LatencyStats runOneForOneMatchBenchmark(std::size_t matchCount)
    {
        MatchingEngine engine;
        std::vector<uint64_t> latenciesNs;
        latenciesNs.reserve(matchCount);

        for (std::size_t i = 0; i < matchCount; ++i)
        {
            engine.processOrder(makeOrder(static_cast<OrderId>(i + 1),
                                          Side::SELL,
                                          100.0,
                                          100,
                                          1,
                                          static_cast<uint64_t>(i + 1)));
        }

        const auto benchmarkStart = Clock::now();
        for (std::size_t i = 0; i < matchCount; ++i)
        {
            const auto start = Clock::now();
            engine.processOrder(makeOrder(static_cast<OrderId>(matchCount + i + 1),
                                          Side::BUY,
                                          100.0,
                                          100,
                                          1,
                                          static_cast<uint64_t>(matchCount + i + 1)));
            const auto end = Clock::now();
            latenciesNs.push_back(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
        }
        const auto benchmarkEnd = Clock::now();

        const double elapsedSeconds = std::chrono::duration<double>(benchmarkEnd - benchmarkStart).count();
        return summarize(std::move(latenciesNs), elapsedSeconds);
    }

    LatencyStats runMultiInstrumentBenchmark(std::size_t orderCount, uint32_t instrumentCount)
    {
        MatchingEngine engine;
        std::vector<uint64_t> latenciesNs;
        latenciesNs.reserve(orderCount);

        const auto benchmarkStart = Clock::now();
        for (std::size_t i = 0; i < orderCount; ++i)
        {
            const uint32_t instrumentId = static_cast<uint32_t>((i % instrumentCount) + 1);
            const auto start = Clock::now();
            engine.processOrder(makeOrder(static_cast<OrderId>(i + 1),
                                          (i % 2 == 0) ? Side::SELL : Side::BUY,
                                          100.0,
                                          10,
                                          instrumentId,
                                          static_cast<uint64_t>(i + 1)));
            const auto end = Clock::now();
            latenciesNs.push_back(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
        }
        const auto benchmarkEnd = Clock::now();

        const double elapsedSeconds = std::chrono::duration<double>(benchmarkEnd - benchmarkStart).count();
        return summarize(std::move(latenciesNs), elapsedSeconds);
    }
}

int main(int argc, char** argv)
{
    const std::size_t orderCount = (argc > 1) ? std::stoull(argv[1]) : 100000;
    const uint32_t instrumentCount = (argc > 2) ? static_cast<uint32_t>(std::stoul(argv[2])) : 100;

    if (orderCount == 0 || instrumentCount == 0)
    {
        std::cerr << "Usage: latency_test [order_count] [instrument_count]\n";
        return 2;
    }

    std::cout << "Matching engine benchmark: order_count=" << orderCount
              << ", instrument_count=" << instrumentCount << "\n";

    printStats("resting limit inserts", runRestingOrderBenchmark(orderCount));
    printStats("one-for-one matches", runOneForOneMatchBenchmark(orderCount));
    printStats("multi-instrument flow", runMultiInstrumentBenchmark(orderCount, instrumentCount));

    return 0;
}
