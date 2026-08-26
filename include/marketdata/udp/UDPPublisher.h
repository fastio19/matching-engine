#pragma once

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

private:
    std::string group;
    uint16_t port;

    // Opaque socket handle (SOCKET on Windows, int on POSIX stored as intptr_t)
    std::intptr_t socketFd;

    // ---------------- Helpers ----------------

    void initSocket();
    void setupMulticast();
    void serializeAndSend(const Trade& trade);

    std::string serializeTrade(const Trade& trade) const;
};