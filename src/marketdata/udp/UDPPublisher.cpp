// UDPPublisher.cpp

#include "marketdata/udp/UDPPublisher.h"

#include <sstream>
#include <stdexcept>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace
{
#ifdef _WIN32
    SOCKET toNativeSocket(std::intptr_t socketFd)
    {
        return static_cast<SOCKET>(socketFd);
    }
#else
    int toNativeSocket(std::intptr_t socketFd)
    {
        return static_cast<int>(socketFd);
    }
#endif
}

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
UDPPublisher::UDPPublisher(const std::string& multicastGroup,
                           uint16_t multicastPort)
    : group(multicastGroup),
      port(multicastPort),
      socketFd(static_cast<std::intptr_t>(-1))
{
#ifdef _WIN32
    WSADATA wsaData;

    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    initSocket();
    setupMulticast();
}

// ------------------------------------------------------------
// Destructor
// ------------------------------------------------------------
UDPPublisher::~UDPPublisher()
{
#ifdef _WIN32
    if (toNativeSocket(socketFd) != INVALID_SOCKET)
    {
        closesocket(toNativeSocket(socketFd));
    }

    WSACleanup();
#else
    if (toNativeSocket(socketFd) >= 0)
    {
        close(toNativeSocket(socketFd));
    }
#endif
}

// ------------------------------------------------------------
// publishTrade
// ------------------------------------------------------------
void UDPPublisher::publishTrade(const Trade& trade)
{
    serializeAndSend(trade);
}

void UDPPublisher::publishLastTradedPrice(uint32_t instrumentId,
                                          Price lastTradedPrice,
                                          uint64_t timestamp)
{
    serializeAndSendLastTradedPrice(instrumentId, lastTradedPrice, timestamp);
}

void UDPPublisher::publishBookUpdate(uint32_t instrumentId,
                                     std::optional<Price> bestBid,
                                     std::optional<Price> bestAsk,
                                     uint64_t timestamp)
{
    serializeAndSendBookUpdate(instrumentId, bestBid, bestAsk, timestamp);
}

// ------------------------------------------------------------
// initSocket
// ------------------------------------------------------------
void UDPPublisher::initSocket()
{
#ifdef _WIN32
    SOCKET nativeSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (nativeSocket == INVALID_SOCKET)
    {
        throw std::runtime_error("Failed to create UDP socket");
    }
    socketFd = static_cast<std::intptr_t>(nativeSocket);
#else
    int nativeSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (nativeSocket < 0)
    {
        throw std::runtime_error("Failed to create UDP socket");
    }
    socketFd = static_cast<std::intptr_t>(nativeSocket);
#endif
}

// ------------------------------------------------------------
// setupMulticast
// ------------------------------------------------------------
void UDPPublisher::setupMulticast()
{
    int ttl = 1;

    if (setsockopt(toNativeSocket(socketFd),
                   IPPROTO_IP,
                   IP_MULTICAST_TTL,
                   reinterpret_cast<char*>(&ttl),
                   sizeof(ttl)) < 0)
    {
        throw std::runtime_error("Failed to set multicast TTL");
    }
}

// ------------------------------------------------------------
// serializeTrade
// ------------------------------------------------------------
std::string UDPPublisher::serializeTrade(const Trade& trade) const
{
    std::ostringstream oss;

    oss << "{"
        << "\"eventType\":\"TRADE\","
        << "\"tradeId\":" << trade.tradeId << ","
        << "\"buyOrderId\":" << trade.buyOrderId << ","
        << "\"sellOrderId\":" << trade.sellOrderId << ","
        << "\"instrumentId\":" << trade.instrumentId << ","
        << "\"price\":" << trade.price << ","
        << "\"quantity\":" << trade.quantity << ","
        << "\"timestamp\":" << trade.timestamp
        << "}";

    return oss.str();
}

std::string UDPPublisher::serializeLastTradedPrice(uint32_t instrumentId,
                                                   Price lastTradedPrice,
                                                   uint64_t timestamp) const
{
    std::ostringstream oss;

    oss << "{"
        << "\"eventType\":\"LAST_TRADED_PRICE\","
        << "\"instrumentId\":" << instrumentId << ","
        << "\"lastTradedPrice\":" << lastTradedPrice << ","
        << "\"timestamp\":" << timestamp
        << "}";

    return oss.str();
}

std::string UDPPublisher::serializeBookUpdate(uint32_t instrumentId,
                                              std::optional<Price> bestBid,
                                              std::optional<Price> bestAsk,
                                              uint64_t timestamp) const
{
    std::ostringstream oss;

    oss << "{"
        << "\"eventType\":\"BOOK_UPDATE\","
        << "\"instrumentId\":" << instrumentId << ","
        << "\"bestBid\":" << bestBid.value_or(0.0) << ","
        << "\"bestAsk\":" << bestAsk.value_or(0.0) << ","
        << "\"timestamp\":" << timestamp
        << "}";

    return oss.str();
}

// ------------------------------------------------------------
// serializeAndSend
// ------------------------------------------------------------
void UDPPublisher::serializeAndSend(const Trade& trade)
{
    std::string payload = serializeTrade(trade);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, group.c_str(), &addr.sin_addr) != 1)
    {
        throw std::runtime_error("Invalid multicast group address: " + group);
    }

    int bytesSent = sendto(
        toNativeSocket(socketFd),
        payload.c_str(),
        static_cast<int>(payload.size()),
        0,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    );

    if (bytesSent < 0)
    {
        throw std::runtime_error("Failed to send UDP multicast packet");
    }
}

void UDPPublisher::serializeAndSendLastTradedPrice(uint32_t instrumentId,
                                                   Price lastTradedPrice,
                                                   uint64_t timestamp)
{
    std::string payload = serializeLastTradedPrice(instrumentId, lastTradedPrice, timestamp);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, group.c_str(), &addr.sin_addr) != 1)
    {
        throw std::runtime_error("Invalid multicast group address: " + group);
    }

    int bytesSent = sendto(
        toNativeSocket(socketFd),
        payload.c_str(),
        static_cast<int>(payload.size()),
        0,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    );

    if (bytesSent < 0)
    {
        throw std::runtime_error("Failed to send UDP multicast packet");
    }
}

void UDPPublisher::serializeAndSendBookUpdate(uint32_t instrumentId,
                                              std::optional<Price> bestBid,
                                              std::optional<Price> bestAsk,
                                              uint64_t timestamp)
{
    std::string payload = serializeBookUpdate(instrumentId, bestBid, bestAsk, timestamp);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, group.c_str(), &addr.sin_addr) != 1)
    {
        throw std::runtime_error("Invalid multicast group address: " + group);
    }

    int bytesSent = sendto(
        toNativeSocket(socketFd),
        payload.c_str(),
        static_cast<int>(payload.size()),
        0,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    );

    if (bytesSent < 0)
    {
        throw std::runtime_error("Failed to send UDP multicast packet");
    }
}