#pragma once
#include <cstdint>
#include "Types.h"

struct Order {
    uint8_t id;              // unique order id
    double price;            // ignored for MARKET orders
    uint32_t quantity;       // remaining qty
    Side side;               // buy/sell
    OrderType type;          // limit/market
    Market market;           // NSE / BSE
    uint64_t timestamp;      // for price-time priority
    uint32_t instrumentId;   // stock id
};
