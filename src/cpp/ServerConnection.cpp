#include "ServerConnection.hpp"
#include "SettingsFileManager.hpp"
#include "errorLogger.h"
#include <ws2tcpip.h>
#include <cassert>
#include <optional>
#include <format>
#include <future>
#include <string>
#include <array>
#include <chrono>
#include <utility>

static void logLastError()
{
#ifdef _WIN32

    int const ec {WSAGetLastError()};

    //10054 = The connection was forcibly closed by the remote host.
    if(ec == 10054)
        return;

    LPSTR msg {nullptr};
    FormatMessageA
    (
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM     |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        ec,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msg),
        0,
        nullptr
    );

    FileErrorLogger::get().log(msg);
    LocalFree(msg);

#else
    char msg[strerrorlen_s(errno)]{};
    strerror_s(msg, sizeof(msg), errno);
    FileErrorLogger::get().log(msg);
#endif
}

ServerConnection::ServerConnection(std::function<void()> onConnect, std::function<void()> onDisconnect)
    : mOnConnect{std::move(onConnect)}, mOnDisconnect{std::move(onDisconnect)}
{
    mReadBuff.resize(mReadBuffInitialSize);

    if(int result {WSAStartup(MAKEWORD(2, 2), &mWinSockData)}; result == 0)
        connectToServerAsync();
    else
        FileErrorLogger::get().log(std::format("WSAStartup() failed with error {}", result));
}

ServerConnection::~ServerConnection()
{
    closesocket(mSocket);
    WSACleanup();
}

//Gets any new data that might have been sent from the server.
void ServerConnection::update()
{
    if( ! mConnectToServerThreadIsDone ) [[unlikely]]
    {
        auto const futureStatus { mFutureSocket.wait_for(std::chrono::microseconds(100)) };
        if(futureStatus == std::future_status::ready)
        {
            auto const maybeSocket {mFutureSocket.get()};
            if(maybeSocket)
            {
                mSocket = *maybeSocket;
                mIsConnected = true;
                mOnConnect();
            }
            
            mConnectToServerThreadIsDone = true;
        }

        return;
    }

    if( ! mIsConnected ) { return; }

    fd_set sockSet;
    FD_ZERO(&sockSet);
    FD_SET(mSocket, &sockSet);
    timeval selectWaitTime{.tv_sec = 0, .tv_usec = 0};

    int selectResult {select(0, &sockSet, nullptr, nullptr, &selectWaitTime)};
    
    if(selectResult == SOCKET_ERROR)
    {
        logLastError();
        disconnect();
        return;
    }
    else if(selectResult > 0)//select() has indicated that there is a message on the socket ready to be read.
    {
        int const recvResult { recv(mSocket, mReadBuff.data(), 
            static_cast<int>(mReadBuff.size()), 0) };

        if(recvResult == SOCKET_ERROR)
        {
            logLastError();
            disconnect();
        }
        else if(recvResult == 0)//The connection has been gracefully closed.
        {
            disconnect();
        }
    }
}

std::optional<std::byte> ServerConnection::peek(uint32_t idx)
{
    if(idx >= mReadBuff.size())
        return std::nullopt;

    return static_cast<std::byte>(mReadBuff[idx]);
}

//Call ServerConnection::update() first so that you have the most recent data from the socket.
std::optional<std::vector<std::byte>> ServerConnection::read(std::size_t len)
{
    if(mReadBuff.size() < len)
        return std::nullopt;

    std::vector<std::byte> ret 
    {
        reinterpret_cast<std::byte*>(mReadBuff.data()), 
        reinterpret_cast<std::byte*>(mReadBuff.data() + len)
    };

    mReadBuff.erase(mReadBuff.begin(), mReadBuff.begin() + len);

    return std::make_optional(std::move(ret));
}

void ServerConnection::write(std::span<std::byte> buffer)
{
    int32_t numBytesSent {0}, numBytesToSend {static_cast<int32_t>(buffer.size())};
    while(numBytesSent < buffer.size())
    {
        int res {send(mSocket, reinterpret_cast<char*>(buffer.data() + numBytesSent),
            numBytesToSend, 0)};

        if(res == SOCKET_ERROR)
        {
            logLastError();
            disconnect();
            return;
        }

        numBytesSent += res;
        numBytesToSend -= res;
    }
}

//arguments pass by value to avoid any potential race conditions
//fname is the .txt file where the server address should be stored (ServerIP.txt)
static std::optional<SOCKET> connectToServerImpl(std::filesystem::path fname, 
    std::string defaultPort, std::string defaultIP);

void ServerConnection::connectToServerAsync()
{
    if(mIsConnected) { return; }

    mFutureSocket = std::async
    (
        std::launch::async, 
        connectToServerImpl, 
        mServerAddrFileName, 
        mDefaultServerPortStr, 
        mDefaultServerIpStr
    );
}

static bool verifyFilePort(std::string_view port)
{
    if(port.size() > 5 || port.size() < 1)
        return false;

    for(auto c : port)
    {
        if( ! isdigit(c) )
            return false;
    }

    uint64_t const bigPort {std::strtoull(port.data(), nullptr, 10)};

    if(bigPort > (1 << 16) - 1)//if bigger than 65535
        return false;

    return true;
}

static bool verifyFileIP(std::string_view ip)
{
    char dummyBuffer[sizeof(in6_addr)] {};
    return inet_pton(AF_INET,  ip.data(), dummyBuffer) == 0 ||
           inet_pton(AF_INET6, ip.data(), dummyBuffer) == 0;
}

//Generates a new ServerIP.txt file. If ServerIP.txt already exists, it generate a new one.
static void generateDefaultServerIPFile(SettingsManager& manager, 
    std::string_view defaultPort, std::string_view defaultIP)
{
    std::array<std::string, 8> const comments
    {
        "Enter the IP and PORT of the chess server below.",
        "The server source can be found at https://github.com/oskarGrr/chessServer if you want",
        "to build it and set up your own server. The server uses a lot of win32 api calls, so it",
        "will only work on windows.",
        "Be sure to put a space after IP: and PORT: below.",
        "If you accidentally modify this file, and need a new one you can delete it",
        "and a new one will be genertated when you start the application again.",
        "by default this will be the loopback IP. Change it to your server's IP and port."
    };

    std::array<SettingsManager::KVPair, 2> const kvPairs
    {
        SettingsManager::KVPair{"IP", defaultIP.data()},
        SettingsManager::KVPair{"PORT", defaultPort.data()}
    };

    manager.generateNewFile(comments, kvPairs);
}

static void handleSettingsFileErr(SettingsManager::Error const& err, SettingsManager& manager,
    std::string_view defaultIP, std::string_view defaultPort)
{
    FileErrorLogger::get().log(err.msg);

    switch(err.code)
    {
    case SettingsManager::Error::Code::FILE_NOT_FOUND: [[fallthrough]];
    case SettingsManager::Error::Code::KEY_NOT_FOUND:
    {
        manager.deleteFile();
        generateDefaultServerIPFile(manager, defaultPort, defaultIP);
        break;
    }
    }
}

static std::optional<std::string> getPortFromFile(std::filesystem::path const& fname,
    std::string_view defaultPort, std::string_view defaultIP)
{
    SettingsManager manager {fname};

    //Try to get the port from the settings txt file.
    auto maybeFilePort {manager.getValue("PORT")};
    if( ! maybeFilePort )
    {
        handleSettingsFileErr(maybeFilePort.error(), manager, defaultIP, defaultPort);
        return std::nullopt;
    }

    if(verifyFilePort(*maybeFilePort))
        return std::make_optional(std::move(*maybeFilePort));

    auto const msg {std::format("The PORT value in {} is invalid", fname.string())};
    FileErrorLogger::get().log(msg);
    return std::nullopt;
}

static std::optional<std::string> getIPFromFile(std::filesystem::path const& fname, 
    std::string_view defaultPort, std::string_view defaultIP)
{
    SettingsManager manager {fname};

    auto maybeFileIP {manager.getValue("IP")};
    if( ! maybeFileIP )
    {
        handleSettingsFileErr(maybeFileIP.error(), manager, defaultIP, defaultPort);
        return std::nullopt;
    }

    if(verifyFileIP(*maybeFileIP))
        return std::make_optional(std::move(*maybeFileIP));

    auto const msg {std::format("The IP value in {} is invalid", fname.string())};
    FileErrorLogger::get().log(msg);
    return std::nullopt;
}

//arguments pass by value to avoid any potential race conditions
//fname is the .txt file where the server address should be stored (ServerIP.txt)
static std::optional<SOCKET> connectToServerImpl(std::filesystem::path fname,
    std::string defaultPort, std::string defaultIP)
{
    addrinfo hints
    {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };

    addrinfo* addrList {nullptr};

    //Try to get the ip and port from mServerAddrFileName settings .txt file.
    auto const maybeFilePort {getPortFromFile(fname, defaultPort, defaultIP)};
    auto const maybeFileIP   {getIPFromFile(fname, defaultPort, defaultIP)};

    int getaddrinfoResult = getaddrinfo
    (
        maybeFileIP   ? maybeFileIP->c_str()   : defaultIP.data(),
        maybeFilePort ? maybeFilePort->c_str() : defaultPort.data(),
        &hints,
        &addrList
    );

    if(getaddrinfoResult != 0)
    {
        logLastError();
        return std::nullopt;
    }

    SOCKET sock {INVALID_SOCKET};
    for(auto list{addrList}; list; list = list->ai_next)
    {
        if((sock = socket(list->ai_family, list->ai_socktype, 0)) == INVALID_SOCKET)
        {
            logLastError();
            continue;
        }

        if(connect(sock, list->ai_addr, static_cast<int>(list->ai_addrlen)) == SOCKET_ERROR)
        {
            logLastError();

            if(closesocket(sock) == SOCKET_ERROR)
                logLastError();

            sock = INVALID_SOCKET;
        }
    }

    freeaddrinfo(addrList);
    
    if(sock == INVALID_SOCKET)
        return std::nullopt;

    return sock;
}

void ServerConnection::disconnect()
{
    closesocket(mSocket);
    mIsConnected = false;
}