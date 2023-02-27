#include "ChessNetworking.h"
#include "ChessApplication.h"

//this is a very crude peer to peer set up for now until I make a server.

//(the person who is the "server" will need to port forward to 
//accept a connection from the person typing in an ip to try to connect to)
//until I make a server instead so no one has to port forward
#define SERVER_PORT 54000 //the port that the person who is the "server" should listen on 

//the max size of ipv4 address as a string plus '\0'
#define IPV4_ADDR_SIZE 16

P2PChessConnection::P2PChessConnection()
    : m_connectType(ConnectionType::INVALID), m_winSockData{},
      m_socket(INVALID_SOCKET), m_ipv4OfPeer{},
      m_wasConnectionLostOrClosed(false)
{
    //init winsock2
    if(WSAStartup(MAKEWORD(2, 2), &m_winSockData))
    {
        std::string msg("WSAStartup() failed with error: ");
        throw networkException(msg.append(std::to_string(WSAGetLastError())));
    }

    if((m_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        std::string msg("socket() failed with error: ");
        throw networkException(msg.append(std::to_string(WSAGetLastError())));
    }
}

void P2PChessConnection::disconnect()
{
    closesocket(m_socket);
    m_connectType = ConnectionType::INVALID;
    //WSAcleanup() called in dtor
}

P2PChessConnection::~P2PChessConnection()
{
    closesocket(m_socket);
    WSACleanup();
}

void P2PChessConnection::connect2Server(std::string_view targetIP)
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

    m_connectType = ConnectionType::CLIENT;
    m_ipv4OfPeer = targetIP;
    auto& app = ChessApp::getApp();
    app.onNewConnection();
}

//wait for the client to connect for the given amount of time before timing out
void P2PChessConnection::waitForClient2Connect(long const seconds, long const ms)
{
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(listenSocket == SOCKET_ERROR)
    {
        std::string msg("socket() failed with error ");
        int winsockError = WSAGetLastError();
        msg.append(std::to_string(winsockError));
        throw networkException(msg, winsockError);
    }

    sockaddr_in hints{.sin_family = AF_INET, .sin_port = htons(SERVER_PORT)};
    hints.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    if(bind(listenSocket, reinterpret_cast<sockaddr*>(&hints), sizeof hints) == SOCKET_ERROR)
    {
        std::string msg("bind() failed with error ");
        int winsockError = WSAGetLastError();
        msg.append(std::to_string(winsockError));
        closesocket(listenSocket);
        throw networkException(msg, winsockError);
    }

    if(listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::string msg("listen() failed with error ");
        int winsockError = WSAGetLastError();
        msg.append(std::to_string(winsockError));
        closesocket(listenSocket);
        throw networkException(msg, winsockError);
    }

    fd_set listenSet;
    timeval acceptWaitTime{.tv_sec = seconds, .tv_usec = ms};
    FD_ZERO(&listenSet);
    FD_SET(listenSocket, &listenSet);
    sockaddr_in clientInfo{};
    int selectResult = select(0, &listenSet, nullptr, nullptr, &acceptWaitTime);

    if(selectResult == SOCKET_ERROR)
    {
        std::string msg("select() failed with error ");
        int winsockError = WSAGetLastError();
        msg.append(std::to_string(winsockError));
        closesocket(listenSocket);
        throw networkException(msg, winsockError);
    }
    else if(selectResult == 0)//select timed out meaning there is no client trying to connect
    {
        closesocket(listenSocket);
        return;
    }
    else//select indicated that a call to accept() will not block and there is a client ready to connect
    {
        if((m_socket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientInfo), 
            nullptr)) == SOCKET_ERROR)
        {
            std::string msg("accept() failed with error ");
            int winsockError = WSAGetLastError();
            msg.append(std::to_string(winsockError));
            closesocket(listenSocket);
            throw networkException(msg, winsockError);
        }
    }

    m_connectType = ConnectionType::SERVER;
    auto& app = ChessApp::getApp();
    m_ipv4OfPeer.resize(IPV4_ADDR_SIZE);
    m_ipv4OfPeer = inet_ntop(AF_INET, &clientInfo.sin_addr, m_ipv4OfPeer.data(), IPV4_ADDR_SIZE);
    app.onNewConnection();
}

//this method with NOT add the NetMessageType to the beginining of the message. that repsonsiblility
//is that of the callee of this method. so at the point that the string view is passed to this method,
//it should already have the whole tcp payload in the right order in memeory ready to be sent.
void P2PChessConnection::sendMessage(NetMessageType msgType, std::string_view msg)
{
    using enum NetMessageType;
    assert(msgType != INVALID);
    auto throwExcept = []{throw networkException("incorrect message size passed to P2PChessConnection::sendMessage()");};
    if(msgType == MOVE && msg.size() != s_moveMessageSize) throwExcept();
    else if(msgType == RESIGN && msg.size() != s_ResignMessageSize) throwExcept();
    else if(msgType == DRAW_OFFER && msg.size() != s_DrawOfferMessageSize) throwExcept();
    else if((msgType == REMATCH_REQUEST || msgType == REMATCH_ACCEPT) && msg.size() != s_rematchMessageSize) throwExcept();
    else if(msgType == WHICH_SIDE && msg.size() != s_WhichSideMessageSize) throwExcept();

    send(m_socket, msg.data(), msg.size(), 0);
}

//returns an string if there is a message ready to be read on the connection socket, otherwise
//returns std::nullopt if there is not a message ready to be read yet. will throw a networkException
//(defined in ChessNetworking.h) if an error occurs or if the connection has been gracefully closed.
//the seconds and ms is the time that this function should wait for a message to be available to be read
//on the connection socket (the amount of time select() waits for). 
std::optional<std::string> P2PChessConnection::recieveMessageIfAvailable(long seconds, long ms)
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
        receivedMessage.resize(s_moveMessageSize);//always try to recv the max num of bytes
        int recvResult = recv(m_socket, receivedMessage.data(), s_moveMessageSize, 0);

        if(recvResult > 0)//recv successfully got a message of recvResult number of bytes
        {
            return receivedMessage;
        }
        else if(recvResult == SOCKET_ERROR)
        {
            std::string throwMsg("recv() failed with error ");
            int winsockErr = WSAGetLastError();
            throw networkException(throwMsg.append(std::to_string(winsockErr)), winsockErr);
        }
        else//if recvResult was 0 that means the connection has been gracefully closed
        {
            m_connectType = ConnectionType::INVALID;//set the connection type to invalid
            m_wasConnectionLostOrClosed = true;
            return std::nullopt;
        }
    }
}