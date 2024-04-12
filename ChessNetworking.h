#pragma once
#include <WS2tcpip.h>
#include <string_view>
#include <optional>
#include <exception>

#include "chessAppLevelProtocol.h"

//how long (in seconds) the request to pair up will last before timing out
#define PAIR_REQUEST_TIMEOUT_SECS 10

class networkException : public std::exception
{
public: 

    networkException(std::string_view msg, int winsockError = 0) 
        : std::exception(msg.data()), m_errorMsg{msg}, m_winsockError(winsockError), 
          m_hasValidWinsockError(false)
    {
        if(winsockError != 0)
            m_hasValidWinsockError = true;
    }

    inline char const* what() const override {return m_errorMsg.c_str();}
    inline int  getWinsockError() const      {return m_winsockError;}
    inline bool hasValidWinsockError() const {return m_hasValidWinsockError;}

private:

    std::string m_errorMsg;
    int  m_winsockError;
    bool m_hasValidWinsockError;//not every networkException has to have a winsock error
};

//a connection to my chess server (a C multithreaded BSD socket server on my github)
class ChessConnection
{
public:

    ChessConnection();
    ~ChessConnection();

private:

    bool        m_isConnected2Server;
    bool        m_isPairedWithOpponent;
    WSADATA     m_winSockData;
    SOCKET      m_socket;
    sockaddr_in m_addressInfo;
    std::string m_opponentIpv4Str;
    std::string m_ipv4OfPotentialOpponentStr;
    in_addr     m_ipv4OfPotentialOpponent;//in network byte order

    void connect2Server(std::string_view serverIp);

public:

    void disconnectFromServer();
    void sendMessage(std::string_view msg);
    std::optional<std::string> recieveMessageIfAvailable(long seconds = 0, long ms = 0);//waits a given time for a msg
    inline bool isConnectedToServer() const {return m_isConnected2Server;}
    inline bool isPairedWithOpponent() const {return m_isPairedWithOpponent;}
    inline void setIsPairedWithOpponent(bool isPaired) {m_isPairedWithOpponent = isPaired;}
    inline std::string const& getIpv4OfPotentialOpponentStr() const {return m_ipv4OfPotentialOpponentStr;}
    inline in_addr getIpv4OfPotentialOpponent() const {return m_ipv4OfPotentialOpponent;}
    void setPotentialOpponent(in_addr ipv4NwByteOrder);
};