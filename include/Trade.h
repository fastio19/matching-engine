#pragma once
#include <cstdint>

struct Trade {
    uint64_t tradeId;

    uint64_t buyOrderId;
    uint64_t sellOrderId;

    uint32_t instrumentId;

    double price;
    uint32_t quantity;

    uint64_t timestamp;
};