#include "api/SimpleHttpServer.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
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

    void closeSocket(std::intptr_t socketFd)
    {
        closesocket(toNativeSocket(socketFd));
    }
#else
    int toNativeSocket(std::intptr_t socketFd)
    {
        return static_cast<int>(socketFd);
    }

    void closeSocket(std::intptr_t socketFd)
    {
        close(toNativeSocket(socketFd));
    }
#endif

    bool socketIsValid(std::intptr_t socketFd)
    {
#ifdef _WIN32
        return toNativeSocket(socketFd) != INVALID_SOCKET;
#else
        return toNativeSocket(socketFd) >= 0;
#endif
    }

    std::string reasonPhrase(int statusCode)
    {
        switch (statusCode)
        {
        case 200:
            return "OK";
        case 202:
            return "Accepted";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 503:
            return "Service Unavailable";
        default:
            return "Internal Server Error";
        }
    }

    std::size_t parseContentLength(const std::string& headers)
    {
        const std::string key = "Content-Length:";
        const auto pos = headers.find(key);
        if (pos == std::string::npos)
        {
            return 0;
        }

        const auto valueStart = pos + key.size();
        const auto valueEnd = headers.find("\r\n", valueStart);
        return static_cast<std::size_t>(std::stoul(headers.substr(valueStart, valueEnd - valueStart)));
    }
}

SimpleHttpServer::SimpleHttpServer(std::string host,
                                   uint16_t port,
                                   std::function<BrokerApiResponse(const std::string&,
                                                                   const std::string&,
                                                                   const std::string&)> handler)
    : host(std::move(host)),
      port(port),
      handler(std::move(handler)),
      running(false),
      serverSocket(static_cast<std::intptr_t>(-1))
{
}

SimpleHttpServer::~SimpleHttpServer()
{
    stop();
}

void SimpleHttpServer::run()
{
    initSocket();
    running = true;

    while (running)
    {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientAddrLen = sizeof(clientAddr);
#else
        socklen_t clientAddrLen = sizeof(clientAddr);
#endif
        const auto clientSocket = accept(toNativeSocket(serverSocket),
                                         reinterpret_cast<sockaddr*>(&clientAddr),
                                         &clientAddrLen);

#ifdef _WIN32
        if (clientSocket == INVALID_SOCKET)
#else
        if (clientSocket < 0)
#endif
        {
            if (running)
            {
                throw std::runtime_error("Failed to accept HTTP client");
            }
            break;
        }

        handleClient(static_cast<std::intptr_t>(clientSocket));
    }
}

void SimpleHttpServer::stop()
{
    running = false;
    closeServerSocket();
}

void SimpleHttpServer::initSocket()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    const auto nativeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (nativeSocket == INVALID_SOCKET)
#else
    if (nativeSocket < 0)
#endif
    {
        throw std::runtime_error("Failed to create HTTP socket");
    }

    serverSocket = static_cast<std::intptr_t>(nativeSocket);

    int reuse = 1;
    setsockopt(toNativeSocket(serverSocket),
               SOL_SOCKET,
               SO_REUSEADDR,
               reinterpret_cast<char*>(&reuse),
               sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    {
        throw std::runtime_error("Invalid HTTP host address: " + host);
    }

    if (bind(toNativeSocket(serverSocket),
             reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) < 0)
    {
        throw std::runtime_error("Failed to bind HTTP socket");
    }

    if (listen(toNativeSocket(serverSocket), SOMAXCONN) < 0)
    {
        throw std::runtime_error("Failed to listen on HTTP socket");
    }
}

void SimpleHttpServer::closeServerSocket()
{
    if (socketIsValid(serverSocket))
    {
        closeSocket(serverSocket);
        serverSocket = static_cast<std::intptr_t>(-1);
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

void SimpleHttpServer::handleClient(std::intptr_t clientSocket)
{
    std::string request;
    char buffer[4096];

    while (request.find("\r\n\r\n") == std::string::npos)
    {
        const int bytesRead = recv(toNativeSocket(clientSocket), buffer, sizeof(buffer), 0);
        if (bytesRead <= 0)
        {
            closeSocket(clientSocket);
            return;
        }
        request.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    const auto headerEnd = request.find("\r\n\r\n");
    const std::string headers = request.substr(0, headerEnd + 4);
    const std::size_t contentLength = parseContentLength(headers);
    const std::size_t bodyStart = headerEnd + 4;

    while (request.size() - bodyStart < contentLength)
    {
        const int bytesRead = recv(toNativeSocket(clientSocket), buffer, sizeof(buffer), 0);
        if (bytesRead <= 0)
        {
            closeSocket(clientSocket);
            return;
        }
        request.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    std::istringstream requestLine(headers.substr(0, headers.find("\r\n")));
    std::string method;
    std::string path;
    std::string version;
    requestLine >> method >> path >> version;

    const BrokerApiResponse apiResponse = handler(method,
                                                  path,
                                                  request.substr(bodyStart, contentLength));

    std::ostringstream response;
    response << "HTTP/1.1 " << apiResponse.statusCode << " " << reasonPhrase(apiResponse.statusCode) << "\r\n"
             << "Content-Type: " << apiResponse.contentType << "\r\n"
             << "Content-Length: " << apiResponse.body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << apiResponse.body;

    const std::string responseText = response.str();
    send(toNativeSocket(clientSocket),
         responseText.c_str(),
         static_cast<int>(responseText.size()),
         0);
    closeSocket(clientSocket);
}
