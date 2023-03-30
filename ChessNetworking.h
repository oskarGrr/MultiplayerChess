#pragma once
#include <WS2tcpip.h>
#include <string_view>
#include <optional>
#include <exception>

//Network message types sent to the server. 
//The first byte of every message will be one of these types.
//The PAIR_REQUEST_MSGTYPE and PAIR_ACCEPT_MSGTYPE 
//defines are present on the server C source code as well
#define WHICH_SIDE_MSGTYPE      1
#define MOVE_MSGTYPE            2
#define RESIGN_MSGTYPE          3
#define DRAW_OFFER_MSGTYPE      4
#define REMATCH_REQUEST_MSGTYPE 5
#define REMATCH_ACCEPT_MSGTYPE  6
#define PAIR_REQUEST_MSGTYPE    7
#define PAIR_ACCEPT_MSGTYPE     8

//byte 0 is the WHICH_SIDE_MSG. byte 1 is Side::WHTE or Side::BLACK
#define WHICH_SIDE_MSG_SIZE 2

//the layout of the MOVE_MSG
//|0|1|2|3|4|5|6|
//byte 0 will be the MOVE_MSG
//byte 1 will be the file (0-7) of the square where the piece is that will be moving
//byte 2 will be the rank (0-7) of the square where the piece is that will be moving
//byte 3 will be the file (0-7) of the square where the piece will be moving to
//byte 4 will be the rank (0-7) of the square where the piece will be moving to
//byte 5 will be the PromoType (enum defined in board.h) of the promotion if there is one
//byte 6 will be the MoveInfo (enum defined in board.h) of the move that is happening
#define MOVE_MSG_SIZE 7

//These 1 byte sized messages are just their corresponding MSGTYPE
//and thats all. No extra information needed.
#define RESIGN_MSG_SIZE          1
#define DRAW_OFFER_MSG_SIZE      1
#define REMATCH_REQUEST_MSG_SIZE 1
#define REMATCH_ACCEPT_MSG_SIZE  1

//The size of an IPV4 plus the first byte which is 
//PAIR_REQUEST_MSGTYPE or PAIR_ACCEPT_MSGTYPE respectively.
#define PAIR_REQUEST_MSG_SIZE    5
#define PAIR_ACCEPT_MSG_SIZE     5

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

//a connection to my chess server (a C multithreaded socket server on my github)
class ChessConnection
{
public:

    ChessConnection();
    ~ChessConnection();

private:

    bool        m_isConnected;
    bool        m_wasConnectionLostOrClosed;
    WSADATA     m_winSockData;
    SOCKET      m_socket;//used by both peers (server and client) to send and recv chess moves
    std::string m_ipv4OfPeer;

public:

    void sendMessage(std::string_view message);
    void connect2Server(std::string_view targetIP);
    std::optional<std::string> recieveMessageIfAvailable(long seconds = 0, long ms = 0);
    //inline bool isUserConnected() const {return m_connectType != ConnectionType::INVALID;}
    inline std::string const& getIpv4OfPeer() const {return m_ipv4OfPeer;}
    inline bool wasConnectionClosedOrLost() const {return m_wasConnectionLostOrClosed;}
    inline void resetWasConnectionLostBool() {m_wasConnectionLostOrClosed = false;}
};