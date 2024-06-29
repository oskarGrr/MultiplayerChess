#pragma once
#include "moveType.h"
#include "ServerConnection.hpp"
#include "chessNetworkProtocol.h"
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

    //Events that this class publishes:
    //Inc is short for incomming. These messages signify that this type
    //of message has been recieved from the server.
    struct IncMoveEvent : Event {Move move;};
    struct IncResignEvent : Event {};
    struct IncDrawOfferEvent : Event {};
    struct IncDrawDeclineEvent : Event {};
    struct IncPairRequestEvent : Event {uint32_t potentialOpponentID;};
    struct IncRematchRequestEvent : Event {};
    struct IncRematchAcceptEvent : Event {};
    struct IncRematchDeclineEvent : Event {};
    struct IncPairingCompleteEvent : Event {};
    struct IncOpponentClosedConnectionEvent : Event {};
    struct IncUnpairEvent : Event {};
    struct IncNewIDEvent : Event {};
    struct IncIDNotInLobbyEvent : Event {};
    struct IncPairDeclineEvent : Event {};
    struct IncDrawAcceptEvent : Event {};

    using ConnectionManagerEventSystem = EventSystem
    <
        IncMoveEvent,
        IncResignEvent,
        IncDrawOfferEvent,
        IncDrawDeclineEvent,
        IncPairRequestEvent,
        IncRematchRequestEvent,
        IncRematchAcceptEvent,
        IncRematchDeclineEvent,
        IncPairingCompleteEvent,
        IncOpponentClosedConnectionEvent,
        IncUnpairEvent,
        IncNewIDEvent,
        IncIDNotInLobbyEvent,
        IncPairDeclineEvent,
        IncDrawAcceptEvent
    >;

    //Subscribe to one of the event types above. Because of how I designed the event eystem,
    //you can only subscribe with one of the above types through this subscribe function (otherwise it wont compile).
    //if another class has an event system that it uses to publish events, then the event types
    //that it defines will be the only types that you can subscribe to for that instance of the event system.
    template <typename EventType>
    void subscribe(ConnectionManagerEventSystem::OnEventCallback callback)
    {
        mEventSystem.subscribe<EventType>(callback);
    }
    
    //Call once per main loop iteration.
    void update();

    auto isThereAPotentialOpponent() const {return mIsThereAPotentialOpponent;}//Is there a person you are trying to pair with/trying to pair with you.
    auto isPairedWithOpponent() const {return mIsPairedWithOpponent;}
    auto getPotentialOpponentsID() const {return mPotentialOpponentID;}
    auto getUniqueID() const {return mUniqueID;}
    auto getOpponentID() const {return mOpponentID;}
    static bool isOpponentIDStringValid(std::string_view opponentID);

private:

    bool        mIsPairedWithOpponent {false};
    bool        mIsThereAPotentialOpponent {false};//Is there a person you are trying to pair with/trying to pair with you.
    uint32_t    mPotentialOpponentID {0};//The ID if someone attempting to play chess with you.
    uint32_t    mUniqueID {0};//This clients unique ID that the server provided upon connection.
    uint32_t    mOpponentID {0};

    ServerConnection mServerConn;

    ConnectionManagerEventSystem mEventSystem;

private:

    using NetworkMessage  = std::vector<std::byte>;
    using NetworkMessages = std::vector<NetworkMessage>;

    std::optional<NetworkMessages> retrieveNetworkMessages();

    //Helper to reduce processNetworkMessages() size.
    void processNetworkMessage(NetworkMessage const&);//Helper to reduce processNetworkMessages() size.
    void processNetworkMessages();//Processes incoming network messages.
    void buildAndSendMoveMsgType(Move const& move, PromoType const& pt);
    void buildAndSendPairRequest(std::string_view potentialOpponent);
    void buildAndSendPairAccept();
    void buildAndSendPairDecline();
    void sendHeaderOnlyMessage(MessageType msgType, MessageSize msgSize);
    
    template <typename EventType>
    void publishEventNoData();

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

public:
    ConnectionManager()=default;
    ~ConnectionManager()=default;
    ConnectionManager(ConnectionManager const&)=delete;
    ConnectionManager(ConnectionManager&&)=delete;
    ConnectionManager& operator=(ConnectionManager const&)=delete;
    ConnectionManager& operator=(ConnectionManager&&)=delete;
};