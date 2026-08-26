#pragma once

#include <vector>
#include <memory>
#include <mutex>

#include "core/Trade.h"
#include "marketdata/IMarketDataPublisher.h"

// ------------------------------------------------------------
// MarketDataEngine
// - Fan-out layer for all market data events (Trades, etc.)
// - Routes events to multiple publishers (UDP, Kafka, etc.)
// - Core abstraction used by MatchingEngine
// ------------------------------------------------------------
class MarketDataEngine {
public:
    MarketDataEngine() = default;
    ~MarketDataEngine() = default;

    // Disable copy/move (engine-like class safety)
    MarketDataEngine(const MarketDataEngine&) = delete;
    MarketDataEngine& operator=(const MarketDataEngine&) = delete;

    // ------------------- Publisher Management -------------------

    // Register a new market data publisher (UDP / Kafka / future WS)
    void addPublisher(std::shared_ptr<IMarketDataPublisher> publisher);

    // Remove all publishers (useful for testing / reset)
    void clearPublishers();

    // ------------------- Event API -------------------

    // Main entry point: publish a trade event to all subscribers
    void publishTrade(const Trade& trade);

private:
    // List of all downstream consumers
    std::vector<std::shared_ptr<IMarketDataPublisher>> publishers;

    // Protect fan-out in case engine becomes multi-threaded later
    std::mutex mtx;
};