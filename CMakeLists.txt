cmake_minimum_required(VERSION 3.16)
project(MatchingEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include headers
include_directories(include)

if(WIN32)
    set(VCPKG_X64_WINDOWS_ROOT D:/Projects/matching-engine/vcpkg_installed/x64-windows)
    include_directories(${VCPKG_X64_WINDOWS_ROOT}/include)
    include_directories(D:/Projects/matching-engine/vcpkg_tool/installed/x64-windows/include)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RDKAFKA REQUIRED rdkafka)
    pkg_check_modules(PQXX REQUIRED libpqxx)
endif()

# ---------------- ENGINE LIBRARY ----------------
add_library(engine
    src/orderbook/OrderBook.cpp
    src/orderbook/MatchingEngine.cpp
    src/utils/TimeUtils.cpp
    src/utils/IdGenerator.cpp
)

# ---------------- RECOVERY LIBRARY ----------------
add_library(recovery
    src/recovery/FileOrderStateStore.cpp
)

target_link_libraries(recovery PRIVATE nlohmann_json::nlohmann_json engine)

# ---------------- API LIBRARY ----------------
add_library(api
    src/api/BrokerApiHandler.cpp
    src/api/SimpleHttpServer.cpp
)

target_link_libraries(api PRIVATE nlohmann_json::nlohmann_json engine)

# ---------------- MARKETDATA LIBRARY ----------------
add_library(marketdata
    src/marketdata/db/PostgreSQLTradeStore.cpp
    src/marketdata/kafka/KafkaPublisher.cpp
    src/marketdata/kafka/TradeDBConsumer.cpp
    src/marketdata/kafka/KafkaOrderCommandConsumer.cpp
    src/marketdata/kafka/KafkaOrderCommandPublisher.cpp
    src/marketdata/kafka/KafkaMatchingRunner.cpp
    src/marketdata/udp/UDPPublisher.cpp
)

target_link_libraries(marketdata PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(marketdata PRIVATE recovery)
if(WIN32)
    target_link_libraries(marketdata PRIVATE
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/rdkafka.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/rdkafka.lib>
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/pqxx.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/pqxx.lib>
    )
else()
    target_include_directories(marketdata PRIVATE ${RDKAFKA_INCLUDE_DIRS} ${PQXX_INCLUDE_DIRS})
    target_link_libraries(marketdata PRIVATE ${RDKAFKA_LIBRARIES} ${PQXX_LIBRARIES})
endif()

# ---------------- MAIN APP ----------------
add_executable(main_app
    src/main.cpp
)

target_link_libraries(main_app engine)

add_executable(latency_test
    benchmarks/latency_test.cpp
)

target_link_libraries(latency_test PRIVATE engine)

add_executable(kafka_matching_app
    src/kafka_main.cpp
)

target_link_libraries(kafka_matching_app PRIVATE engine marketdata)
if(WIN32)
    target_link_libraries(kafka_matching_app PRIVATE
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/rdkafka.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/rdkafka.lib>
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/pqxx.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/pqxx.lib>
    )
    add_custom_command(TARGET kafka_matching_app POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -Dtarget_dir=$<TARGET_FILE_DIR:kafka_matching_app>
            -Dsource_dir=$<IF:$<CONFIG:Debug>,${VCPKG_X64_WINDOWS_ROOT}/debug/bin,${VCPKG_X64_WINDOWS_ROOT}/bin>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake
    )
endif()

add_executable(broker_api_app
    src/broker_api_main.cpp
)

target_link_libraries(broker_api_app PRIVATE api marketdata engine)
if(WIN32)
    target_link_libraries(broker_api_app PRIVATE
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/rdkafka.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/rdkafka.lib>
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/pqxx.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/pqxx.lib>
    )
    add_custom_command(TARGET broker_api_app POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -Dtarget_dir=$<TARGET_FILE_DIR:broker_api_app>
            -Dsource_dir=$<IF:$<CONFIG:Debug>,${VCPKG_X64_WINDOWS_ROOT}/debug/bin,${VCPKG_X64_WINDOWS_ROOT}/bin>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake
    )
endif()

add_executable(kafka_smoke_test
    src/kafka_smoke_test.cpp
)

target_link_libraries(kafka_smoke_test PRIVATE engine marketdata)
if(WIN32)
    target_link_libraries(kafka_smoke_test PRIVATE
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/rdkafka.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/rdkafka.lib>
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/pqxx.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/pqxx.lib>
    )
    add_custom_command(TARGET kafka_smoke_test POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -Dtarget_dir=$<TARGET_FILE_DIR:kafka_smoke_test>
            -Dsource_dir=$<IF:$<CONFIG:Debug>,${VCPKG_X64_WINDOWS_ROOT}/debug/bin,${VCPKG_X64_WINDOWS_ROOT}/bin>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake
    )
endif()

add_executable(trade_db_smoke_test
    src/trade_db_smoke_test.cpp
)

target_link_libraries(trade_db_smoke_test PRIVATE marketdata engine)
if(WIN32)
    target_link_libraries(trade_db_smoke_test PRIVATE
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/rdkafka.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/rdkafka.lib>
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/pqxx.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/pqxx.lib>
    )
    add_custom_command(TARGET trade_db_smoke_test POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -Dtarget_dir=$<TARGET_FILE_DIR:trade_db_smoke_test>
            -Dsource_dir=$<IF:$<CONFIG:Debug>,${VCPKG_X64_WINDOWS_ROOT}/debug/bin,${VCPKG_X64_WINDOWS_ROOT}/bin>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake
    )
endif()

add_executable(trade_db_consumer_app
    src/trade_db_consumer_main.cpp
)

target_link_libraries(trade_db_consumer_app PRIVATE marketdata engine)
if(WIN32)
    target_link_libraries(trade_db_consumer_app PRIVATE
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/rdkafka.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/rdkafka.lib>
        $<$<CONFIG:Debug>:${VCPKG_X64_WINDOWS_ROOT}/debug/lib/pqxx.lib>
        $<$<CONFIG:Release>:${VCPKG_X64_WINDOWS_ROOT}/lib/pqxx.lib>
    )
    add_custom_command(TARGET trade_db_consumer_app POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -Dtarget_dir=$<TARGET_FILE_DIR:trade_db_consumer_app>
            -Dsource_dir=$<IF:$<CONFIG:Debug>,${VCPKG_X64_WINDOWS_ROOT}/debug/bin,${VCPKG_X64_WINDOWS_ROOT}/bin>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake
    )
endif()

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
    tests/OrderBookTest.cpp
    tests/BrokerApiHandlerTest.cpp
    tests/FileOrderStateStoreTest.cpp
    tests/KafkaSchemaTest.cpp
    tests/KafkaOrderCommandConsumerTest.cpp
    tests/KafkaFlowTest.cpp
    tests/KafkaBookPublishingTest.cpp
    tests/KafkaLtpPublishingTest.cpp
    tests/KafkaTradePublishingTest.cpp
)

target_include_directories(engine_tests PRIVATE ${googletest_SOURCE_DIR}/googletest/include)
target_link_libraries(engine_tests PRIVATE
    nlohmann_json::nlohmann_json
    api
    engine
    recovery
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_tests)