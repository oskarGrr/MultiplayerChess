#pragma once
#include <WinSock2.h>
#include <span>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>
#include <future>
#include <functional>

class ServerConnection
{
public:

    //Gets any new data that might have been sent from the server.    
    void update();
    
    //Peek into the buffer of bytes received from the server.
    //std::nullopt returned when idx is an invalid index.
    std::optional<std::byte> peek(uint32_t idx);

    //Call getDataFromServer() first so that you have the most recent data from the socket.
    std::optional<std::vector<std::byte>> read(std::size_t len);

    void write(std::span<std::byte>);
    auto isConnected() const {return mIsConnected;}

    ServerConnection(std::function<void()> onConnect, std::function<void()> onDisconnect);
    ~ServerConnection();

private:

    void connectToServerAsync();
    void disconnect();

    static constexpr auto mReadBuffInitialSize {256};
    std::vector<char> mReadBuff;

    std::future<std::optional<SOCKET>> mFutureSocket;
    SOCKET mSocket {INVALID_SOCKET};
    WSADATA mWinSockData {};

    //If ServerIP.txt could not be found or there was some IO error with it,
    //then a new one is made. These will be the default port and ip values written to it.
    std::string const mDefaultServerPortStr {"42069"};
    std::string const mDefaultServerIpStr   {"127.0.0.1"};
    std::string const mServerAddrFileName   {"ServerIP.txt"};

    bool mIsConnected {false};
    bool mConnectToServerThreadIsDone {false};

    std::function<void()> mOnConnect;
    std::function<void()> mOnDisconnect;

public:
    ServerConnection(ServerConnection const&)=delete;
    ServerConnection(ServerConnection&&)=delete;
    ServerConnection& operator=(ServerConnection const&)=delete;
    ServerConnection& operator=(ServerConnection&&)=delete;
};