#include "ChessNetworking.h"
#include "ChessApplication.h"
#include <fstream>
#include <iostream>
#include <cassert>


//(the person who is the "server" will need to port forward to 
//accept a connection from the person typing in an ip to try to connect to)
#define SERVER_PORT 54000 //the port that the person who is the "server" should listen on 

P2PChessConnection::P2PChessConnection() 
    : m_netType(NetType::INVALID), m_winSockData{}, m_connectionSocket(INVALID_SOCKET)
{
    //init winsock2
    if(WSAStartup(MAKEWORD(2, 2), &m_winSockData))
    {
        std::ofstream ofs("log.txt");
        std::cerr << "WSAStartup() failed with error: "
                  << WSAGetLastError() << ". error logged in log.txt" << '\n';
        ofs << "WSAStartup() failed with error: " << WSAGetLastError() << '\n';
    }

    if((m_connectionSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        std::ofstream ofs("log.txt");
        std::cerr << "socket() failed with error: "
                  << WSAGetLastError() << ". error logged in log.txt\n";
        ofs << "socket() failed with error: " << WSAGetLastError() << '\n';
    }
}

P2PChessConnection::~P2PChessConnection()
{
    closesocket(m_connectionSocket);
    WSACleanup();
}

//if this function returns a string with a success message to be displayed,
//then the connection was successful. otherwise the function will throw an string as an error
std::string P2PChessConnection::connect2Server(std::string_view targetIP)
{   
    m_netType = NetType::CLIENT;

    sockaddr_in targetAddrInfo{.sin_family = AF_INET, .sin_port = htons(SERVER_PORT)};
    auto ptonResult = inet_pton(AF_INET, targetIP.data(), &targetAddrInfo.sin_addr);

    if(ptonResult == 0)
    {
        m_netType = NetType::INVALID;
        return std::string("the address entered is not a valid IPv4 dotted-decimal string\n"
                           "a correct IPv4 example: 147.202.234.122");
    }
    else if(ptonResult < 0) 
    {
        std::string ret("inet_pton() failed with error ");
        ret.append(std::to_string(WSAGetLastError()));
        m_netType = NetType::INVALID;
        return ret;
    }

    if(connect(m_connectionSocket, reinterpret_cast<sockaddr*>(&targetAddrInfo),
        static_cast<int>(sizeof targetAddrInfo)) == SOCKET_ERROR)
    {
        std::string ret("connect() failed with error: ");
        ret.append(std::to_string(WSAGetLastError()));
        m_netType = NetType::INVALID;
        return ret;
    }

    std::string successMsg("successfully connected to ");
    successMsg.append(targetIP);
    return successMsg;
}

//this will be started up in another thread
std::string P2PChessConnection::waitForClient2Connect()
{
    m_netType = NetType::SERVER;

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(listenSocket == SOCKET_ERROR)
    {
        std::string ret("socket() failed with error ");
        ret.append(std::to_string(WSAGetLastError()));
        m_netType = NetType::INVALID;
        return ret;
    }

    sockaddr_in hints{.sin_family = AF_INET, .sin_port = htons(SERVER_PORT)};
    hints.sin_addr.S_un.S_addr = INADDR_ANY;
    if(bind(listenSocket, reinterpret_cast<sockaddr*>(&hints), sizeof hints) == SOCKET_ERROR)
    {
        std::string ret("bind() failed with error ");
        ret.append(std::to_string(WSAGetLastError()));
        closesocket(listenSocket);
        m_netType = NetType::INVALID;
        return ret;
    }

    if(listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::string ret("listen() failed with error ");
        ret.append(std::to_string(WSAGetLastError()));
        closesocket(listenSocket);
        m_netType = NetType::INVALID;
        return ret;
    }

    fd_set listenSet;
    timeval acceptWaitTime{.tv_sec = 20};
    FD_ZERO(&listenSet);
    FD_SET(listenSocket, &listenSet);
    sockaddr_in clientInfo{};
    int selectResult = select(0, &listenSet, nullptr, nullptr, &acceptWaitTime);

    if(selectResult == SOCKET_ERROR)
    {
        std::string ret("select() failed with error ");
        ret.append(std::to_string(WSAGetLastError()));
        closesocket(listenSocket);
        m_netType = NetType::INVALID;
        return ret;
    }
    else if(selectResult == 0)//select timed out meaning there is no client trying to connect
    {
        closesocket(listenSocket);
        m_netType = NetType::INVALID;
        return std::string("there is no client trying to connect currently");
    }
    else//select indicated that a call to accept() will not block and there is a client ready to connect
    {
        if((m_connectionSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientInfo), 
            nullptr)) == SOCKET_ERROR)
        {
            std::string ret("accept() failed with error ");
            ret.append(std::to_string(WSAGetLastError()));
            closesocket(listenSocket);
            m_netType = NetType::INVALID;
            return ret;
        }
    }

    std::string clientIP;
    clientIP.resize(256);
    if(inet_ntop(AF_INET, &clientInfo.sin_addr, clientIP.data(), clientIP.size()))
    { 
        std::string ret("client successfully connected from ");
        ret.append(clientIP);
        return ret;
    }
}

void P2PChessConnection::sendMessage(NetMessageTy messageType, std::string_view message)
{
    char sendBuffer[s_messageSize];
    std::memset(sendBuffer, 0, s_messageSize);

    //copy the message type into the send buffer
    std::memcpy(sendBuffer, &messageType, sizeof NetMessageTy);

    //copy the message into the send buffer. "step" over the message type in the buffer and then copy
    //exactly s_messageSize - the sizeof a message type bytes (5 bytes) from the view into the send buffer
    std::memcpy(sendBuffer + sizeof NetMessageTy, message.data(), s_messageSize - sizeof NetMessageTy);

    send(m_connectionSocket, sendBuffer, s_messageSize, 0);
}

//will throw a string with a helpful message and (if applicable) a winsock error message.
//returns the message received as a std::string if there is a message ready to be read on the
//connection socket otherwise returns an empty optional (std::nullopt)
std::optional<std::string> P2PChessConnection::recieveMessageIfAvailable()
{
    fd_set sockSet;
    FD_ZERO(&sockSet);
    FD_SET(m_connectionSocket, &sockSet);
    timeval selectWaitTime{};//tell select() to wait the minimum amount of time
    int selectResult = select(0, &sockSet, nullptr, nullptr, &selectWaitTime);

    if(selectResult == SOCKET_ERROR)
    {
        std::string ret("select() failed with error ");
        ret.append(std::to_string(WSAGetLastError()));
        throw ret;
    }
    else if(selectResult == 0)//select has timed out meaning there is no message to read yet
    {
        return std::nullopt;
    }
    else//select has indicated that there is a message on the connection socket ready to be read...
    {
        std::string receivedMessage;
        receivedMessage.resize(s_messageSize);
        int recvResult = recv(m_connectionSocket, receivedMessage.data(), s_messageSize, 0);

        if(recvResult > 0)//recv successfully got a message of recvResult number of bytes
        {
            return receivedMessage;
        }
        else if(recvResult == SOCKET_ERROR)
        {
            std::string throwMsg("recv() failed with error ");
            throw throwMsg.append(std::to_string(WSAGetLastError()));
        }
        else//if recvResult was 0 that means the connection has been gracefully closed
        {
            m_netType = NetType::INVALID;
        }   
    }
}