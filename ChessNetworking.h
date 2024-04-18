#pragma once
#include <WS2tcpip.h>
#include <string_view>
#include <optional>
#include <exception>
#include <vector>

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

    ChessConnection(ChessConnection const&) = delete;
    ChessConnection(ChessConnection&&) = delete;
    ChessConnection& operator=(ChessConnection const&) = delete;
    ChessConnection& operator=(ChessConnection&&) = delete;

private:

    bool        m_isConnected2Server;
    bool        m_isPairedWithOpponent;
    bool        m_isThereAPotentialOpponent;//Is there a person you are trying to pair with/trying to pair with you.
    WSADATA     m_winSockData;
    SOCKET      m_socket;
    sockaddr_in m_addressInfo;
    uint32_t    m_potentialOpponentID;//The ID if someone attempting to play chess with you.
    uint32_t    m_uniqueID;//This clients unique ID that the server provided upon connection.
    uint32_t    m_opponentID;

    void connect2Server(std::string_view serverIp);

public:

    static bool isOpponentIDStringValid(std::string_view opponentID);

    void disconnectFromServer();
    void sendMessage(const char* msg, std::size_t msgSize);
    std::optional<std::vector<char>> recieveMessageIfAvailable(long seconds = 0, long ms = 0);//waits a given time for a network msg

    void setIsThereAPotentialOpponent(bool isThereAPotentialOpponent) {m_isThereAPotentialOpponent = isThereAPotentialOpponent;}
    void setIsPairedWithOpponent(bool isPaired) {m_isPairedWithOpponent = isPaired;}
    void setPotentialOpponentID(uint32_t potentialOppoentID) {m_potentialOpponentID = potentialOppoentID;}
    void setUniqueID(uint32_t ID) {m_uniqueID = ID;}
    void setOpponentID(uint32_t ID) {m_opponentID = ID;}

    auto isThereAPotentialOpponent() const {return m_isThereAPotentialOpponent;}//Is there a person you are trying to pair with/trying to pair with you.
    auto isConnectedToServer() const {return m_isConnected2Server;}
    auto isPairedWithOpponent() const {return m_isPairedWithOpponent;}
    auto getPotentialOpponentsID() const {return m_potentialOpponentID;}
    auto getUniqueID() const {return m_uniqueID;}
    auto getOpponentID() const {return m_opponentID;}
};