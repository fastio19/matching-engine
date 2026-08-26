cmake_minimum_required(VERSION 3.16)
project(MatchingEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include headers
include_directories(include)
include_directories(D:/Projects/matching-engine/vcpkg_tool/installed/x64-windows/include)

# Find vcpkg dependencies
# find_package(libpqxx REQUIRED)  # Install libpqxx separately, e.g., via PostgreSQL installer or vcpkg when fixed
# find_package(nlohmann_json REQUIRED)  # Using FetchContent instead

# ---------------- ENGINE LIBRARY ----------------
add_library(engine
    src/orderbook/OrderBook.cpp
    src/orderbook/MatchingEngine.cpp
    src/utils/TimeUtils.cpp
    src/utils/IdGenerator.cpp
)

# ---------------- MARKETDATA LIBRARY ----------------
add_library(marketdata
    src/marketdata/kafka/TradeDBConsumer.cpp
)

target_link_libraries(marketdata PRIVATE nlohmann_json::nlohmann_json)

# ---------------- MAIN APP ----------------
add_executable(main_app
    src/main.cpp
)

target_link_libraries(main_app engine)

# ---------------- GOOGLE TEST ----------------
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(googletest nlohmann_json)

enable_testing()

add_executable(engine_tests
    tests/MatchingEngineTest.cpp
)

target_include_directories(engine_tests PRIVATE ${googletest_SOURCE_DIR}/googletest/include)

target_link_libraries(engine_tests
    engine
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_tests)