#pragma once
#include "ChessMove.hpp"
#include "ServerConnection.hpp"
#include "ChessEvents.hpp"
#include <string_view>

//how long (in seconds) the request to pair up will last before timing out
#define PAIR_REQUEST_TIMEOUT_SECS 10

//This class is responsible for constructing/deconstructing messages from the server.
//The class ServerConnection is the more lower level TCP socket networking class that is generally completely abstracted from the game of chess completely.
//If you wanted to test/try different lower level network implementations you could switch from composing ServerConnection directly into this class,
//to using depencency injection and just holding on to a unique_ptr to a polymorphic interface with like read() and wrtie() virtual methods.
class ConnectionManager
{
public:

    ConnectionManager(NetworkEventSystem::Publisher const&, GUIEventSystem::Subscriber&, 
        BoardEventSystem::Subscriber&);

    ~ConnectionManager();
    
    //Call once per main loop iteration.
    void update();

    auto isConnectedToServer() const {return mServerConn.isConnected();}
    auto isThereAPotentialOpponent() const {return mIsThereAPotentialOpponent;}//Is there a person you are trying to pair with/trying to pair with you.
    auto isPairedOnline() const {return mIsPairedWithOpponent;}
    auto getPotentialOpponentsID() const {return mPotentialOpponentID;}
    auto getUniqueID() const {return mUniqueID;}
    auto getOpponentID() const {return mOpponentID;}
    static bool isOpponentIDStringValid(std::string_view opponentID);

private:

    bool     mIsPairedWithOpponent {false};
    bool     mIsThereAPotentialOpponent {false};//Is there a person you are trying to pair with/trying to pair with you.
    uint32_t mPotentialOpponentID {0};//The ID if someone attempting to play chess with you.
    uint32_t mUniqueID {0};//This clients unique ID that the server provided upon connection.
    uint32_t mOpponentID {0};

    ServerConnection mServerConn;

    enum struct GuiSubscriptions
    {
        PAIR_REQUEST,
        PAIR_ACCEPT,
        PAIR_DECLINE,

        DRAW_OFFER,
        DRAW_ACCEPT,
        DRAW_DECLINE,

        REMATCH_REQUEST,
        REMATCH_ACCEPT,
        REMATCH_DECLINE,

        RESIGN,
        UNPAIR
    };

    //Manager for the many GUI events
    SubscriptionManager<GuiSubscriptions, GUIEventSystem::Subscriber> mGuiEventSubManager;

    NetworkEventSystem::Publisher const& mNetworkEventPublisher;

    BoardEventSystem::Subscriber& mBoardEventSubscriber;
    SubscriptionID mMoveCompletedSubID {INVALID_SUBSCRIPTION_ID};

private:

    using NetworkMessage  = std::vector<std::byte>;
    using NetworkMessages = std::vector<NetworkMessage>;

    void onConnect();
    void onDisconnect();

    std::optional<NetworkMessages> retrieveNetworkMessages();

    //Helper to reduce processNetworkMessages() size.
    void processNetworkMessage(NetworkMessage const&);//Helper to reduce processNetworkMessages() size.
    void processNetworkMessages();//Processes incoming network messages.
    void buildAndSendMoveMsgType(ChessMove const& move);
    void buildAndSendPairRequest(uint32_t potentialOpponent);
    void buildAndSendPairAccept();
    void buildAndSendPairDecline();
    void sendHeaderOnlyMessage(MessageType msgType);
    
    template<typename EventT, typename... EventArgs>
    void pubEvent(EventArgs&&...);

    //helper method to reduce ctor size
    void subToEvents();

    //The message types that do not have a corresponding handle message 
    //function simply call publishEventNoData() inside of processNetworkMessage().
    void handlePairRequestMessage(NetworkMessage const&);
    void handlePairingCompleteMessage(NetworkMessage const&);
    void handleMoveMessage(NetworkMessage const&);
    void handleUnpairMessage();
    void handleInvalidMessageType();
    void handleRematchDeclineMessage();
    void handleOpponentClosedConnectionMessage();
    void handleNewIDMessage(NetworkMessage const&);
    void handlePairDeclineMessage(NetworkMessage const&);
    void handleIDNotInLobbyMessage(NetworkMessage const& msg);

public:
    ConnectionManager(ConnectionManager const&)=delete;
    ConnectionManager(ConnectionManager&&)=delete;
    ConnectionManager& operator=(ConnectionManager const&)=delete;
    ConnectionManager& operator=(ConnectionManager&&)=delete;
};