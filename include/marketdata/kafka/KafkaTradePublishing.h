#pragma once

#include "marketdata/kafka/KafkaSchema.h"
#include "marketdata/IMarketDataPublisher.h"
#include "orderbook/MatchingEngine.h"

inline void wireTradePublishing(MatchingEngine& engine, IMarketDataPublisher& publisher)
{
    engine.setTradeHandler([&publisher](const Trade& trade)
    {
        publisher.publishTrade(trade);
    });
}

inline void wireLastTradedPricePublishing(MatchingEngine& engine, IMarketDataPublisher& publisher)
{
    engine.setLtpHandler([&publisher](uint32_t instrumentId, Price lastTradedPrice, uint64_t timestamp)
    {
        publisher.publishLastTradedPrice(instrumentId, lastTradedPrice, timestamp);
    });
}

inline void wireBookPublishing(MatchingEngine& engine, IMarketDataPublisher& publisher)
{
    engine.setBookHandler([&publisher](uint32_t instrumentId,
                                       std::optional<Price> bestBid,
                                       std::optional<Price> bestAsk,
                                       uint64_t timestamp)
    {
        publisher.publishBookUpdate(instrumentId, bestBid, bestAsk, timestamp);
    });
}
