#include "ChessNetworking.h"
#include "ChessApplication.h"
#include "errorLogger.h"
#include <optional>
#include <sstream>
#include <thread>
#include <fstream>
#include <filesystem>
#include <string>

#define SERVER_PORT 42069

ChessConnection::ChessConnection()
    : m_isConnectedToServer{false},
      m_isPairedWithOpponent{false}, 
      m_isThereAPotentialOpponent{false},
      m_winSockData{}, 
      m_socket{INVALID_SOCKET},
      m_addressInfo{},
      m_potentialOpponentID{},
      m_uniqueID{},
      m_opponentID{},
      m_serverIPFileName{"ServerIP.txt"}
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

    std::thread conn2ServerThread(&ChessConnection::connectToServer, this);
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
    m_isConnectedToServer = false;
    setIsPairedWithOpponent(false);
    setIsThereAPotentialOpponent(false);
}

bool ChessConnection::loadServerIPFromFile()
{
    if(!std::filesystem::exists(m_serverIPFileName))
        generateServerIPFile();

    std::ifstream ifs{m_serverIPFileName};

    if(!ifs.is_open())
        return false;

    bool wasIPCorrect = false;
    bool wasPORTCorrect = false;

    for(std::string line; std::getline(ifs, line) && (!wasIPCorrect || !wasPORTCorrect);)
    {
        //If the current line is a comment or an empty line.
        if(line[0] == '#' || line.size() == 0)
            continue;

        std::size_t const notWhiteSpace = line.find_first_not_of("\t ");

        //If the line is only whitespace (tabs or spaces).
        if(notWhiteSpace == std::string::npos)
            continue;

        std::size_t const colonIndex = line.find_first_of(':');

        //No colon? UNACCEPTABLE!!!1
        if(colonIndex == std::string::npos)
            return false;

        std::string rightSide;
        std::string leftSide;//should be "IP" or "PORT"
        try 
        {
            leftSide = line.substr(notWhiteSpace, colonIndex);
            rightSide = line.substr(line.find_first_not_of("\t ", colonIndex + 1));
        }
        catch(std::out_of_range const& e)
        {
            return false;
        }

        if(leftSide == "PORT")
        {
            wasPORTCorrect = checkServerPortTxt(rightSide);
            if(wasPORTCorrect) std::cout << rightSide << '\n';
        }
        else if(leftSide == "IP")
        {
            wasIPCorrect = checkServerIPTxt(rightSide);
            if(wasIPCorrect) std::cout << rightSide << '\n';
        }
        else//If leftSide was not "IP" or "PORT"
        {
            return false;
        }
    }

    return wasIPCorrect && wasPORTCorrect;
}

//Checks the PORT field in ServerIP.txt.
bool ChessConnection::checkServerPortTxt(std::string_view port)
{
    //The Biggest port string size should be 5 chars.
    if(port.size() > 5)
        return false;
    
    for(auto c : port)
    {
        if(!std::isdigit(c))
            return false;
    }
    
    uint64_t const portValue = std::stoull(static_cast<std::string>(port));
    
    if(portValue > (1 << 16) - 1)
        return false;
    
    m_addressInfo.sin_port = htons(portValue);
    return true;
}

//Checks the IP line in ServerIP.txt.
bool ChessConnection::checkServerIPTxt(std::string_view ip)
{
    if(inet_pton(AF_INET, ip.data(), &m_addressInfo.sin_addr) == 1)
    {
        m_addressInfo.sin_family = AF_INET;
        return true;
    }
    if(inet_pton(AF_INET6, ip.data(), &m_addressInfo.sin_addr) == 1)
    {
        m_addressInfo.sin_family = AF_INET6;
        return true;
    }

    return false;
}

//If ServerIP.txt does not exist, this creates a new one.
void ChessConnection::generateServerIPFile()
{
    assert(!std::filesystem::exists(m_serverIPFileName));
    std::ofstream ofs{m_serverIPFileName};

    ofs << "#Enter the IP and PORT of the chess server below." << std::endl
        << "#The server source can be found at https://github.com/oskarGrr/chessServer if you want" << std::endl
        << "#to build it and set up your own server. The server uses a lot of win32 api calls, so it" << std::endl
        << "#will only work on windows. " << std::endl
        << std::endl
        << "#Be sure to put a space after IP: and PORT: below." << std::endl
        << "#If you accidentally modify this file, and need a new one you can delete it" << std::endl
        << "#and a new one will be genertated when you start the application again." << std::endl
        << std::endl
        << "#by default this will be the loopback IP. Change it to your server's IP and port." << std::endl
        << std::endl
        << "IP: 127.0.0.1" << std::endl
        << "PORT: 42069" << std::endl;
}

void ChessConnection::connectToServer()
{
    assert(!m_isConnectedToServer);

    if(!loadServerIPFromFile())
    {
        //open popup to explain that ServerIP.txt was missing/invalid.
        //ask if they want to input the servers ip in popup.
        return;
    }

    if(connect(m_socket, reinterpret_cast<sockaddr*>(&m_addressInfo),
        static_cast<int>(sizeof m_addressInfo)) == SOCKET_ERROR)
    {
        std::string errMsg {"connect() failed with winsock error: "};
        errMsg.append(std::to_string(WSAGetLastError()));
        FileErrorLogger::get().logErrors(errMsg);
    }
    else m_isConnectedToServer = true;
}

bool ChessConnection::isOpponentIDStringValid(std::string_view opponentID)
{
    //The ID is a uint32_t as a string in base 10. The string referenced by
    //opponentID should not exceed 10 chars. '\0' is not counted by size().
    if(opponentID.size() > 10 || opponentID.size() == 0)
        return false;

    //if any of the chars are not 0-9
    for(auto const& c : opponentID)
    {
        if( !std::isdigit(c) )
            return false;
    }

    //Why do string to ___ functions take std::string const& instead of std::string_view :(
    uint64_t bigID = std::stoull(std::string(opponentID));

    if(bigID > UINT32_MAX) 
        return false;

    return true;
}

//this method wont prefix the message with the MSGTYPE
//(MSGTYPE's are defined in chessAppLevelProtocol.h). It just sends msg to the server
void ChessConnection::sendMessage(const char* msg, std::size_t msgSize)
{
    send(m_socket, msg, msgSize, 0);
}

//returns a vector of chars if there is a message ready to be read on the connection socket, otherwise
//returns std::nullopt if there is not a message ready to be read yet. will throw a networkException
//(defined in ChessNetworking.h) if an error occurs or if the connection has been gracefully closed.
//the seconds and ms params are the time that this function should wait for a message to be available to be read
//on the connection socket (the amount of time select() waits for).
std::optional<std::vector<char>> ChessConnection::recieveMessageIfAvailable(long seconds, long ms)
{
    fd_set sockSet;
    FD_ZERO(&sockSet);
    FD_SET(m_socket, &sockSet);
    timeval selectWaitTime{.tv_sec = seconds, .tv_usec = ms};

    int selectResult = select(0, &sockSet, nullptr, nullptr, &selectWaitTime);
    
    if(selectResult == SOCKET_ERROR)
    {
        std::string errMsg("select() failed with error ");
        int winsockErr = WSAGetLastError();
        errMsg.append(std::to_string(winsockErr));
        throw networkException(errMsg, winsockErr);
    }
    else if(selectResult == 0)//select has timed out meaning there is no message to read yet
    {
        return std::nullopt;
    }
    else//select has indicated that there is a message on the connection socket ready to be read...
    {
        //128 bytes should be enough until I implement a chat room in the chess game :D.
        //I have an assert below though to make sure the size of a message is not greater than 128.
        std::vector<char> msgBuffer;
        msgBuffer.resize(128);
    
        int recvResult = recv(m_socket, msgBuffer.data(), msgBuffer.size(), 0); 
        if(recvResult > 0)//recv() successfully got a message of recvResult number of bytes
        {
            assert(recvResult <= msgBuffer.size());

            //now that we know the num of bytes to read, "shrink" (the size not the capacity)
            //of the vector so that msgBuffer.size() holds the correct msg size.
            msgBuffer.resize(recvResult);
            
            return std::make_optional(msgBuffer);
        }
        else if(recvResult == SOCKET_ERROR)
        {
            std::string errMsg("recv() failed with error ");
            int const winsockErr = WSAGetLastError();

            //10054 an existing connection was forcibly closed by the remote host
            if(winsockErr == 10054)
            {
                //We are already disconnected, but let's make it official!
                disconnectFromServer();
                return std::nullopt;
            }
            else throw networkException(errMsg, winsockErr);
        }
        else//if recvResult is 0 that means the connection has been gracefully closed
        {
            disconnectFromServer();
            return std::nullopt;
        }
    }

    return std::nullopt;
}