 
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

Kafka design
------------
- orders.commands: place/cancel/modify commands keyed by instrumentId
- orders.trades: matched trade events keyed by instrumentId
- orders.book: book snapshots / top-of-book updates
- orders.dlq: malformed or unprocessable messages
- KafkaOrderCommandConsumer: parses commands and dispatches place/cancel/modify callbacks
- kafka_matching_app: runnable Kafka-backed matcher entrypoint
- UDP multicast: broadcasts the latest traded price to connected brokers/listeners

Scaling model
-------------
- Partition `orders.commands` by `instrumentId`
- Run multiple `kafka_matching_app` replicas in the same consumer group
- Each replica gets a subset of partitions, so instruments are handled in parallel
- Keep the partition count above the number of matcher replicas you want to run

State recovery
--------------
`kafka_matching_app` keeps a durable JSON snapshot of open resting limit orders.
On startup it loads the snapshot before consuming new Kafka commands, so a restart
does not start with an empty order book.

The snapshot is updated after every place, modify, and cancel command handled by
the matcher. Fully filled orders are removed from the snapshot because they no
longer rest on the book.

Run the matcher with an explicit recovery file:

`kafka_matching_app localhost:9092 orders.commands matching-engine-group orders.trades orders.book 239.0.0.1 5000 matching-engine-state.json`

Important production note: each matcher replica should use its own snapshot file
or persistent volume for the Kafka partitions it owns. Do not share one snapshot
file across multiple running matcher replicas.

API to Kafka flow
-----------------
1. Client or broker app calls the order API.
2. API validates the request and converts it into an order command.
3. The command is published to `orders.commands` keyed by `instrumentId`.
4. `KafkaOrderCommandConsumer` reads the command and forwards it to `MatchingEngine`.
5. `MatchingEngine` matches the order in the correct per-instrument `OrderBook`.
6. Trades are published to `orders.trades` and book snapshots to `orders.book`.
7. The latest traded price is multicast over UDP for fast broker fan-out.
8. Downstream consumers can persist trades, show live market data, or trigger alerts.

Broker API
----------
Run the broker-facing HTTP API:

`broker_api_app localhost:9092 orders.commands 0.0.0.0 8080`

Arguments:
1. Kafka broker list, default `localhost:9092`
2. Kafka command topic, default `orders.commands`
3. HTTP bind address, default `0.0.0.0`
4. HTTP port, default `8080`

Health check:

`curl http://localhost:8080/health`

Place order:

`curl -X POST http://localhost:8080/orders -H "Content-Type: application/json" -d "{\"orderId\":101,\"instrumentId\":1,\"side\":\"BUY\",\"orderType\":\"LIMIT\",\"price\":105.5,\"quantity\":100}"`

Modify order:

`curl -X PUT http://localhost:8080/orders -H "Content-Type: application/json" -d "{\"orderId\":101,\"instrumentId\":1,\"side\":\"BUY\",\"orderType\":\"LIMIT\",\"price\":106.0,\"quantity\":50}"`

Cancel order:

`curl -X DELETE http://localhost:8080/orders -H "Content-Type: application/json" -d "{\"orderId\":101,\"instrumentId\":1}"`

Accepted API requests are published to `orders.commands` with `instrumentId` as the Kafka key, so all commands for the same stock stay ordered in the same partition.

Example messages
----------------
orders.commands:
{"eventType":"ORDER_PLACE","orderId":101,"instrumentId":1,"side":"BUY","orderType":"LIMIT","price":105.5,"quantity":100,"timestamp":1724670000000}

orders.trades:
{"eventType":"TRADE","tradeId":9001,"buyOrderId":101,"sellOrderId":202,"instrumentId":1,"price":105.0,"quantity":50,"timestamp":1724670000001}

Live smoke test
---------------
Run `kafka_smoke_test` against a live broker to verify the full command -> match -> trade publish path.

Example:
`kafka_smoke_test --debug localhost:9092 orders.commands orders.trades`

It will:
- start the Kafka-backed matcher
- publish a BUY/SELL command pair
- wait for the matching trade on `orders.trades`
- publish book snapshots to `orders.book`
- exit non-zero if Kafka is unreachable or the trade does not arrive in time

Debug mode adds:
- startup/config logging
- librdkafka diagnostics on stderr
- payload prints for sent commands and received trades

Trade DB smoke test
-------------------
Run `trade_db_smoke_test` against a live PostgreSQL database to verify the trade persistence path.

If PostgreSQL is not already running locally, start it with Docker:

`powershell -ExecutionPolicy Bypass -File .\scripts\start-postgres.ps1`

Example:

`trade_db_smoke_test "host=localhost port=5432 dbname=matching_engine user=postgres password=secret"`

It will:
- connect to PostgreSQL
- create the `trades` table if it does not exist
- insert a test trade
- insert the same trade again to verify `ON CONFLICT (trade_id) DO NOTHING`
- confirm exactly one row exists for that trade id
- delete the smoke-test row before exiting

Keep the row for manual inspection:

`trade_db_smoke_test "host=localhost port=5432 dbname=matching_engine user=postgres password=secret" 9000000000001 --keep-row`

For the local Docker database created by `start-postgres.ps1`, use:

`trade_db_smoke_test "host=localhost port=5432 dbname=matching_engine user=postgres password=postgres"`

Stop the local PostgreSQL container later with:

`powershell -ExecutionPolicy Bypass -File .\scripts\stop-postgres.ps1`

Deployment
----------
The application is split into deployable services:

- `broker_api_app`: broker-facing HTTP API. Publishes validated order commands to Kafka.
- `kafka_matching_app`: matching service. Consumes `orders.commands`, updates books, emits trades/book updates/LTP.
- `trade_db_consumer_app`: persistence service. Consumes `orders.trades` and writes to PostgreSQL.

Local full-stack deployment:

1. Copy environment defaults:

`copy .env.example .env`

2. Edit `.env` and change `POSTGRES_PASSWORD` plus `TRADE_DB_CONNECTION_STRING`.

3. Start the full stack:

`powershell -ExecutionPolicy Bypass -File .\scripts\start-stack.ps1`

4. Check the API:

`curl http://localhost:8080/health`

5. Stop the stack:

`powershell -ExecutionPolicy Bypass -File .\scripts\stop-stack.ps1`

Production hardening rules:

- Keep `.env` out of git. It is ignored because it can contain database passwords.
- Use a secret manager for DB credentials in real environments.
- Put `broker_api_app` behind an API gateway or load balancer.
- Add TLS and broker authentication before exposing the API outside a private network.
- Keep Kafka topic partitions greater than or equal to matcher replicas.
- Give each matcher replica its own persistent recovery volume/file.
- Do not share one recovery snapshot file across multiple live matcher replicas.
- Use replicated Kafka and PostgreSQL for high availability; the local Compose file uses single-node services only for development.

Performance testing
-------------------
Build and run the core matching benchmark:

`cmake --build build --config Release --target latency_test`

`.\build\Release\latency_test.exe 100000 100`

Arguments:
1. number of orders per scenario, default `100000`
2. number of instruments for the multi-instrument scenario, default `100`

The benchmark reports average, p50, p95, p99, max latency, and throughput for:
- resting limit inserts
- one-for-one matches
- multi-instrument order flow

The matching engine does not print every trade by default because per-trade stdout
is a major latency bottleneck. The demo `main_app` enables trade logging explicitly.

Observability
-------------
The broker API exposes basic health and metrics endpoints:

`curl http://localhost:8080/health`

`curl http://localhost:8080/metrics`

API metrics include:
- `totalRequests`
- `acceptedCommands`
- `rejectedRequests`
- `publishFailures`

The matching engine tracks internal counters through `MatchingEngine::getMetrics()`:
- accepted and rejected orders
- cancel requests, successful cancels, rejected cancels
- trades generated
- current open resting orders
- last and max order processing latency in nanoseconds

Per-trade console logging is disabled by default to protect latency. Enable it only
for demos or local debugging with `MatchingEngine::setTradeLoggingEnabled(true)`.

Local Kafka setup
-----------------
If Kafka is not already running locally, start it with Docker:

`powershell -ExecutionPolicy Bypass -File .\scripts\start-kafka.ps1`

Stop it later with:

`powershell -ExecutionPolicy Bypass -File .\scripts\stop-kafka.ps1`

This brings up:
- ZooKeeper on `localhost:2181`
- Kafka on `localhost:9092`
- the required topics:
  - `orders.commands`
  - `orders.trades`
  - `orders.book`
  - `orders.dlq`