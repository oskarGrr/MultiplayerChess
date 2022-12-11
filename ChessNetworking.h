#pragma once
#include <WS2tcpip.h>
#include <string_view>
#include <cstdint>
#include <optional>

//for now the multyplayer will be a "peer to peer" direct connection where
//one of the players will need to be the "server" and 
//port forward on port SERVER_PORT (defined in ChessNetworking.cpp)
class P2PChessConnection
{
public:

    P2PChessConnection();
    ~P2PChessConnection();

    enum struct NetMessageTy : uint8_t {INVALID, MOVE, RESIGN, DRAW};//types of messages
    enum struct NetType : uint8_t {INVALID, SERVER, CLIENT};

    //number of bytes of info sent accross the wire in a char array.
    //what will be sent will be a NetMessageTy to signify what the following bytes represent
    static constexpr std::size_t s_messageSize{sizeof(NetMessageTy)};

    NetType m_netType;
    WSADATA m_winSockData;
    SOCKET  m_connectionSocket;//used by both peers (server and client) to send and recv chess moves

    //this will return a string message if something goes wrong which will be a windows 
    //socket error if there is one from WSAGetLastError or another message
    std::string connect2Server(std::string_view targetIP);
    std::string waitForClient2Connect();
    void sendMessage(NetMessageTy messageType, std::string_view message);
    std::optional<std::string> recieveMessageIfAvailable();

    inline bool isUserConnected() const {return m_netType != NetType::INVALID;}
    inline NetType isUserServerOrClient() const {return m_netType;}
};