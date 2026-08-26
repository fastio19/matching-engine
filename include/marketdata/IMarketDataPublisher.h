#pragma once
#include "core/Trade.h"

class IMarketDataPublisher {
public:
    virtual ~IMarketDataPublisher() = default;
    virtual void publishTrade(const Trade& trade) = 0;
};