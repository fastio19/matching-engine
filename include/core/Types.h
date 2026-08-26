#pragma once
#include <cstdint>
 
typedef uint64_t OrderId;
typedef double Price;

enum class Side : uint8_t {
    BUY,
    SELL
};

enum class OrderType : uint8_t {
    LIMIT,
    MARKET
};

enum class TimeInForce : uint8_t {
    DAY,
    IOC
};

enum class Market : uint8_t {
    NSE,
    BSE
};