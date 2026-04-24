 
matching-engine/
├── include/                  # Header files (.h)
│   ├── core/
│   │   ├── Types.h
│   │   ├── Order.h
│   │   ├── Trade.h
│   │
│   ├── orderbook/
│   │   ├── OrderBook.h
│   │   ├── MatchingEngine.h
│   │
│   ├── utils/
│   │   ├── TimeUtils.h
│   │   ├── IdGenerator.h
│
├── src/                      # Implementation files (.cpp)
│   ├── orderbook/
│   │   ├── OrderBook.cpp
│   │   ├── MatchingEngine.cpp
│   │
│   ├── utils/
│   │   ├── TimeUtils.cpp
│   │
│   └── main.cpp
│
├── tests/                    # Unit tests
│   ├── OrderBookTest.cpp
│
├── benchmarks/               # Performance tests (later 🔥)
│   ├── latency_test.cpp
│
├── build/                    # Compiled output (ignored in git)
│
├── CMakeLists.txt            # Build system (later)
├── .gitignore
└── README.md




Incoming Order
      ↓
MatchingEngine.processOrder()
      ↓
matchBuy / matchSell
      ↓
Trades generated
      ↓
Remaining qty → add to OrderBook