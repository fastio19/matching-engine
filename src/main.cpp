#include <iostream>
#include "orderbook/OrderBook.h"
#include "orderbook/MatchingEngine.h"
#include "core/Types.h"
#include "utils/TimeUtils.h"

using namespace std;
int main() {
    MatchingEngine engine;

    // -------- STOCK 1 (instrumentId = 1) --------
    Order o1{1, 101, 50, Side::SELL, OrderType::LIMIT, Market::NSE, TimeUtils::getCurrentTime(), 1};
    Order o2{2, 105, 100, Side::BUY, OrderType::LIMIT, Market::NSE, TimeUtils::getCurrentTime(), 1};

    // -------- STOCK 2 (instrumentId = 2) --------
    Order o3{3, 200, 40, Side::SELL, OrderType::LIMIT, Market::NSE, TimeUtils::getCurrentTime(), 2};
    Order o4{4, 210, 40, Side::BUY, OrderType::LIMIT, Market::NSE, TimeUtils::getCurrentTime(), 2};

    engine.processOrder(o1);
    engine.processOrder(o2);  // matches ONLY with o1

    engine.processOrder(o3);
    engine.processOrder(o4);  // matches ONLY with o3

    engine.printAllBooks();
}