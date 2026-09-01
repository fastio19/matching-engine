#pragma once
#include <cstdint>
#include <optional>

#include "core/Types.h"
#include "core/Trade.h"

class IMarketDataPublisher {
public:
    virtual ~IMarketDataPublisher() = default;
    virtual void publishTrade(const Trade& trade) = 0;
    virtual void publishLastTradedPrice(uint32_t instrumentId,
                                        Price lastTradedPrice,
                                        uint64_t timestamp)
    {
    }
    virtual void publishBookUpdate(uint32_t instrumentId,
                                   std::optional<Price> bestBid,
                                   std::optional<Price> bestAsk,
                                   uint64_t timestamp) = 0;
};