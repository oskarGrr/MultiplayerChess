#include "ChessNetworking.h"
#include "ChessApplication.h"

//this is a very crude peer to peer set up for now until I make a server.

//(the person who is the "server" will need to port forward to 
//accept a connection from the person typing in an ip to try to connect to)
//until I make a server instead so no one has to port forward
#define SERVER_PORT 54000 //the port that the person who is the "server" should listen on 

//the max size of ipv4 address as a string plus '\0'
#define IPV4_ADDR_SIZE 16

ChessConnection::ChessConnection()
    : m_isConnected{false}, m_winSockData{},
      m_socket{INVALID_SOCKET}, m_ipv4OfPeer{},
      m_wasConnectionLostOrClosed{false}
{
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
}

ChessConnection::~ChessConnection()
{
    closesocket(m_socket);
    WSACleanup();
}

void ChessConnection::connect2Server(std::string_view targetIP)
{   
    sockaddr_in targetAddrInfo{.sin_family = AF_INET, .sin_port = htons(SERVER_PORT)};
    auto ptonResult = inet_pton(AF_INET, targetIP.data(), &targetAddrInfo.sin_addr); 

    if(ptonResult == 0)
    {
        throw networkException("the address entered is not a valid IPv4 dotted-decimal string\n"
            "a correct IPv4 example: 192.0.2.146");
    }
    else if(ptonResult < 0)
    {
        std::string msg("inet_pton() failed with error ");
        int winsockError = WSAGetLastError();
        msg.append(std::to_string(winsockError));
        throw networkException(msg, winsockError);
    }

    if(connect(m_socket, reinterpret_cast<sockaddr*>(&targetAddrInfo),
        static_cast<int>(sizeof targetAddrInfo)) == SOCKET_ERROR)
    {
        std::string msg("connect() failed with error: ");
        int winsockError = WSAGetLastError();
        msg.append(std::to_string(winsockError));
        throw networkException(msg, winsockError);
    }

    m_ipv4OfPeer = targetIP;
    auto& app = ChessApp::getApp();
    app.onNewConnection();
}

//this method with NOT add the message type to the beginining of the message. that repsonsiblility
//is that of the callee. so at the point that the string view is passed to this method,
//it should already have the whole tcp payload in the right order in memeory ready to be sent.
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
            m_wasConnectionLostOrClosed = true;
            return std::nullopt;
        }
    }
}