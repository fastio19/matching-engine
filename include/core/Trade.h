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

    Trade(uint64_t buyId,
          uint64_t sellId,
          uint32_t instrId,
          double p,
          uint32_t qty,
          uint64_t ts,
          uint64_t tId)
        : tradeId(tId),
          buyOrderId(buyId),
          sellOrderId(sellId),
          instrumentId(instrId),
          price(p),
          quantity(qty),
          timestamp(ts)
    {}
};