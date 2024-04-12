#include "ChessNetworking.h"
#include "ChessApplication.h"
#include "errorLogger.h"
#include <optional>
#include <thread>
#include <fstream>
#include <string>

#define SERVER_PORT 42069

ChessConnection::ChessConnection()
    : m_isConnected2Server{false}, m_isPairedWithOpponent{false}, 
      m_winSockData{}, m_addressInfo{.sin_family = AF_INET, .sin_port = htons(SERVER_PORT)},
      m_socket{INVALID_SOCKET}, m_opponentIpv4Str{}, m_ipv4OfPotentialOpponentStr{0},
      m_ipv4OfPotentialOpponent{}
{
    m_opponentIpv4Str.resize(INET_ADDRSTRLEN);
    m_ipv4OfPotentialOpponentStr.resize(INET_ADDRSTRLEN);

    //init winsock2
    if(WSAStartup(MAKEWORD(2, 2), &m_winSockData))
    {
        std::string msg("WSAStartup() failed with error: ");
        throw networkException(msg.append(std::to_string(WSAGetLastError())));
    }

    if(m_socket = socket(AF_INET, SOCK_STREAM, 0); m_socket == INVALID_SOCKET)
    {
        std::string msg("socket() failed with error: ");
        throw networkException(msg.append(std::to_string(WSAGetLastError())));
    }

    std::thread conn2ServerThread(&ChessConnection::connect2Server, this, "127.0.0.1");
    conn2ServerThread.detach();//no need to join later :)
}

ChessConnection::~ChessConnection()
{
    closesocket(m_socket);
    WSACleanup();
}

void ChessConnection::disconnectFromServer()
{
    closesocket(m_socket);
    m_isConnected2Server = false;
    m_isPairedWithOpponent = false;
}

static void checkPtonReturn(int ptonResult)
{
    if(ptonResult == 0)
    {
        throw networkException("the address entered is not a"
            "valid IPv4 dotted-decimal string\n"
            "a correct IPv4 example: 192.0.2.146");
    }
    else if(ptonResult < 0)
    {
        std::string msg("inet_pton() failed with error ");
        int winsockError = WSAGetLastError();
        msg.append(std::to_string(winsockError));
        throw networkException(msg, winsockError);
    }
}

void ChessConnection::connect2Server(std::string_view serverIp)
{   
    assert(!m_isConnected2Server);

    checkPtonReturn(inet_pton(AF_INET, serverIp.data(), &m_addressInfo.sin_addr));

    if(connect(m_socket, reinterpret_cast<sockaddr*>(&m_addressInfo),
        static_cast<int>(sizeof m_addressInfo)) == SOCKET_ERROR)
    {
        std::string errMsg {"connect() failed with winsock error: "};
        errMsg.append(std::to_string(WSAGetLastError()));
        FileErrorLogger::get().logErrors(errMsg);
    }
    else m_isConnected2Server = true;
}

void ChessConnection::setPotentialOpponent(in_addr ipv4NwByteOrder)
{
    m_ipv4OfPotentialOpponent = ipv4NwByteOrder;
    inet_ntop(AF_INET, &ipv4NwByteOrder, m_ipv4OfPotentialOpponentStr.data(),
        m_ipv4OfPotentialOpponentStr.size());
}

//this method wont prefix the message with the MSGTYPE
//(MSGTYPE's are defined in chessAppLevelProtocol.h). It just sends msg to the server
void ChessConnection::sendMessage(std::string_view msg)
{
    send(m_socket, msg.data(), msg.size(), 0);
}

//returns an string if there is a message ready to be read on the connection socket, otherwise
//returns std::nullopt if there is not a message ready to be read yet. will throw a networkException
//(defined in ChessNetworking.h) if an error occurs or if the connection has been gracefully closed.
//the seconds and ms is the time that this function should wait for a message to be available to be read
//on the connection socket (the amount of time select() waits for). 
std::optional<std::string> ChessConnection::recieveMessageIfAvailable(long seconds, long ms)
{
    fd_set sockSet;
    FD_ZERO(&sockSet);
    FD_SET(m_socket, &sockSet);
    timeval selectWaitTime{.tv_sec = seconds, .tv_usec = ms};
    int selectResult = select(0, &sockSet, nullptr, nullptr, &selectWaitTime);

    if(selectResult == SOCKET_ERROR)
    {
        std::string ret("select() failed with error ");
        int winsockErr = WSAGetLastError();
        ret.append(std::to_string(winsockErr));
        throw networkException(ret, winsockErr);
    }
    else if(selectResult == 0)//select has timed out meaning there is no message to read yet
    {
        return std::nullopt;
    }
    else//select has indicated that there is a message on the connection socket ready to be read...
    {
        std::string receivedMessage;
        receivedMessage.resize(MOVE_MSG_SIZE);
        int recvResult = recv(m_socket, receivedMessage.data(), receivedMessage.size(), 0);

        if(recvResult > 0)//recv successfully got a message of recvResult number of bytes
        {
            return std::make_optional(receivedMessage);
        }
        else if(recvResult == SOCKET_ERROR)
        {
            std::string throwMsg("recv() failed with error ");
            int winsockErr = WSAGetLastError();
            throw networkException(throwMsg.append(std::to_string(winsockErr)), winsockErr);
        }
        else//if recvResult is 0 that means the connection has been closed
        {
            disconnectFromServer();
            return std::nullopt;
        }
    }
}