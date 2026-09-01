#pragma once

#include <optional>
#include <string>
#include <cstdint>

#include "core/Trade.h"
#include "marketdata/IMarketDataPublisher.h"

// ------------------------------------------------------------
// UDPPublisher
// - Publishes trades over UDP multicast
// - Cross-platform interface
// ------------------------------------------------------------
class UDPPublisher : public IMarketDataPublisher {
public:
    UDPPublisher(const std::string& multicastGroup,
                 uint16_t multicastPort);

    ~UDPPublisher();

    // non-copyable
    UDPPublisher(const UDPPublisher&) = delete;
    UDPPublisher& operator=(const UDPPublisher&) = delete;

    // Interface implementation
    void publishTrade(const Trade& trade) override;
    void publishLastTradedPrice(uint32_t instrumentId,
                                Price lastTradedPrice,
                                uint64_t timestamp) override;
    void publishBookUpdate(uint32_t instrumentId,
                           std::optional<Price> bestBid,
                           std::optional<Price> bestAsk,
                           uint64_t timestamp) override;

private:
    std::string group;
    uint16_t port;

    // Opaque socket handle (SOCKET on Windows, int on POSIX stored as intptr_t)
    std::intptr_t socketFd;

    // ---------------- Helpers ----------------

    void initSocket();
    void setupMulticast();
    void serializeAndSend(const Trade& trade);
    void serializeAndSendLastTradedPrice(uint32_t instrumentId,
                                         Price lastTradedPrice,
                                         uint64_t timestamp);
    void serializeAndSendBookUpdate(uint32_t instrumentId,
                                    std::optional<Price> bestBid,
                                    std::optional<Price> bestAsk,
                                    uint64_t timestamp);

    std::string serializeTrade(const Trade& trade) const;
    std::string serializeLastTradedPrice(uint32_t instrumentId,
                                         Price lastTradedPrice,
                                         uint64_t timestamp) const;
    std::string serializeBookUpdate(uint32_t instrumentId,
                                    std::optional<Price> bestBid,
                                    std::optional<Price> bestAsk,
                                    uint64_t timestamp) const;
};