#include "ConnectionManager.hpp"
#include "chessNetworkProtocol.h"
#include "errorLogger.h"
#include "ChessMove.hpp"
#include <cassert>
#include <optional>
#include <array>
#include <utility>

ConnectionManager::ConnectionManager(NetworkEventSystem::Publisher const& networkEventPublisher,
    GUIEventSystem::Subscriber& guiEventSubscriber, BoardEventSystem::Subscriber& boardEventSubscriber)
        : mGuiEventSubManager    {guiEventSubscriber},
          mNetworkEventPublisher {networkEventPublisher},
          mBoardEventSubscriber  {boardEventSubscriber},
          mServerConn{ [this]{onConnect();}, [this]{onDisconnect();} }
{
    subToEvents();
}

ConnectionManager::~ConnectionManager()
{
    //manually ubsub from BoardEvents::MoveCompleted
    mBoardEventSubscriber.unsub<BoardEvents::MoveCompleted>(mMoveCompletedSubID);

    //mGuiEventSubManager will automatically unsub from the rest of the events...
}

void ConnectionManager::onConnect()
{
    pubEvent<NetworkEvents::ConnectedToServer>();
}

void ConnectionManager::onDisconnect()
{
    mBoardEventSubscriber.unsub<BoardEvents::MoveCompleted>(mMoveCompletedSubID);

    mIsPairedWithOpponent = false;
    mIsThereAPotentialOpponent = false;
}

void ConnectionManager::subToEvents()
{
    mGuiEventSubManager.sub<GUIEvents::PairRequest>(GuiSubscriptions::PAIR_REQUEST, 
        [this](Event const& e){
        auto const& evnt { e.unpack<GUIEvents::PairRequest>() };
        buildAndSendPairRequest(evnt.opponentID);
    });

    mGuiEventSubManager.sub<GUIEvents::PairAccept>(GuiSubscriptions::PAIR_ACCEPT,
        [this](Event const& e){ buildAndSendPairAccept(); });

    mGuiEventSubManager.sub<GUIEvents::PairDecline>(GuiSubscriptions::PAIR_DECLINE, 
        [this](Event const& e){ buildAndSendPairDecline(); });

    mGuiEventSubManager.sub<GUIEvents::DrawOffer>(GuiSubscriptions::DRAW_OFFER,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::DRAW_OFFER_MSGTYPE); });

    mGuiEventSubManager.sub<GUIEvents::DrawAccept>(GuiSubscriptions::DRAW_ACCEPT,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::DRAW_ACCEPT_MSGTYPE); });

    mGuiEventSubManager.sub<GUIEvents::DrawDecline>(GuiSubscriptions::DRAW_DECLINE,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::DRAW_DECLINE_MSGTYPE); });

    mGuiEventSubManager.sub<GUIEvents::RematchRequest>(GuiSubscriptions::REMATCH_REQUEST,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::REMATCH_REQUEST_MSGTYPE); });

    mGuiEventSubManager.sub<GUIEvents::RematchAccept>(GuiSubscriptions::REMATCH_ACCEPT,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::REMATCH_ACCEPT_MSGTYPE); });

    mGuiEventSubManager.sub<GUIEvents::RematchDecline>(GuiSubscriptions::REMATCH_DECLINE,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::REMATCH_DECLINE_MSGTYPE); });

    mGuiEventSubManager.sub<GUIEvents::Resign>(GuiSubscriptions::RESIGN,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::RESIGN_MSGTYPE); });

    mGuiEventSubManager.sub<GUIEvents::Unpair>(GuiSubscriptions::UNPAIR,
        [this](Event const& e){ sendHeaderOnlyMessage(MessageType::UNPAIR_MSGTYPE); });
}

//Call once per main loop iteration.
void ConnectionManager::update()
{
    mServerConn.update();

    if( ! mServerConn.isConnected() ) { return; }

    ////if we are no longer connected
    //if( ! mServerConn.isConnected() )
    //{
    //    pubEvent<NetworkEvents::DisconnectedFromServer>();
    //    return;
    //}

    processNetworkMessages();
}

//helper to reduce processNetworkMessages() size
auto ConnectionManager::retrieveNetworkMessages()
    -> std::optional<NetworkMessages>
{
    NetworkMessages ret;

    //Every message has a two byte header as detailed in chessNetworkProtocol.h.
    auto maybeFirstByte  {mServerConn.peek(0)};
    auto maybeSecondByte {mServerConn.peek(1)};

    while(maybeFirstByte && maybeSecondByte)
    {
        auto const lastByte {mServerConn.peek( static_cast<uint32_t>(*maybeSecondByte) - 1 )};

        //If the server only sent part of the message in the TCP stream.
        if( ! lastByte )
            break;

        auto message {mServerConn.read( static_cast<uint32_t>(*maybeSecondByte) )};

        assert(message);//At this point, this should always be true.

        ret.push_back(std::move(*message));//careful! message variable has been moved from

        //mServerConn.read() will erase the last message. Now peek 0 and 1 will
        //get the next message header if there is one.
        maybeFirstByte  = mServerConn.peek(0);
        maybeSecondByte = mServerConn.peek(1);
    }

    if(ret.size() == 0)
        return std::nullopt;

    return std::make_optional(std::move(ret));
}

void ConnectionManager::handleNewIDMessage(NetworkMessage const& msg)
{
    uint32_t newID{0};
    std::memcpy(&newID, msg.data() + 2, sizeof(newID));
    mUniqueID = ntohl(newID);
    //pubEvent<NetworkInEvents::NewID>(mUniqueID);
}

void ConnectionManager::handlePairDeclineMessage(NetworkMessage const& msg)
{
    //The ID of the player that declined our PAIR_REQUEST_MSGTYPE message.
    uint32_t potentialOpponentID{0};
    std::memcpy(&potentialOpponentID, msg.data() + 2, sizeof(potentialOpponentID));

    mPotentialOpponentID = ntohl(potentialOpponentID);

    pubEvent<NetworkEvents::PairDecline>();

    mIsThereAPotentialOpponent = false;
}

void ConnectionManager::handleIDNotInLobbyMessage(NetworkMessage const& msg)
{
    uint32_t invalidID{0};
    std::memcpy(&invalidID, msg.data() + 2, sizeof(invalidID));

    mIsThereAPotentialOpponent = false;
    
    pubEvent<NetworkEvents::IDNotInLobby>(ntohl(invalidID));
}

//Helper to reduce processNetworkMessages() size.
void ConnectionManager::processNetworkMessage(NetworkMessage const& msg)
{
    //The second byte of the two byte header is the size of the total message.
    assert(static_cast<size_t>(msg.at(1)) == msg.size());

    switch(static_cast<MessageType>(msg[0]))
    {
    using enum MessageType;
    case MOVE_MSGTYPE:             handleMoveMessage(msg);                            break;
    case ID_NOT_IN_LOBBY_MSGTYPE:  handleIDNotInLobbyMessage(msg);                    break;
    case UNPAIR_MSGTYPE:           handleUnpairMessage();                             break;
    case RESIGN_MSGTYPE:           pubEvent<NetworkEvents::OpponentHasResigned>();    break;
    case DRAW_OFFER_MSGTYPE:       pubEvent<NetworkEvents::DrawOffer>();              break;
    case DRAW_DECLINE_MSGTYPE:     pubEvent<NetworkEvents::DrawDeclined>();           break;
    case DRAW_ACCEPT_MSGTYPE:      pubEvent<NetworkEvents::DrawAccept>();             break;
    case REMATCH_ACCEPT_MSGTYPE:   pubEvent<NetworkEvents::RematchAccept>();          break;
    case REMATCH_REQUEST_MSGTYPE:  pubEvent<NetworkEvents::RematchRequest>();         break;
    case PAIR_REQUEST_MSGTYPE:     handlePairRequestMessage(msg);                     break;
    case PAIRING_COMPLETE_MSGTYPE: handlePairingCompleteMessage(msg);                 break;
    case REMATCH_DECLINE_MSGTYPE:  handleRematchDeclineMessage();                     break;
    case NEW_ID_MSGTYPE:           handleNewIDMessage(msg);                           break;
    case PAIR_DECLINE_MSGTYPE:     handlePairDeclineMessage(msg);                     break;
    case OPPONENT_CLOSED_CONNECTION_MSGTYPE: handleOpponentClosedConnectionMessage(); break;
    default: handleInvalidMessageType();
    }
}

//Processes incoming network messages.
void ConnectionManager::processNetworkMessages()
{
    auto maybeMessages {retrieveNetworkMessages()};
    if( ! maybeMessages.has_value() )
        return;

    for(auto const& msg : *maybeMessages)
        processNetworkMessage(msg);
}

void ConnectionManager::handleOpponentClosedConnectionMessage()
{
    mIsPairedWithOpponent = false;
    pubEvent<NetworkEvents::OpponentClosedConnection>();
}

void ConnectionManager::handleRematchDeclineMessage()
{
    mIsPairedWithOpponent = false;
    pubEvent<NetworkEvents::RematchDecline>();
}

void ConnectionManager::handlePairingCompleteMessage(NetworkMessage const& msg)
{
    auto const side { static_cast<Side>(msg[2]) };

    mIsPairedWithOpponent = true;
    mIsThereAPotentialOpponent = false;

    mOpponentID = mPotentialOpponentID;

    mMoveCompletedSubID = mBoardEventSubscriber.sub<BoardEvents::MoveCompleted>(
        [this](Event const& e){
        auto const& evnt { e.unpack<BoardEvents::MoveCompleted>() };
        buildAndSendMoveMsgType(evnt.move);
    });

    pubEvent<NetworkEvents::PairingComplete>(mOpponentID, side);
}

void ConnectionManager::handlePairRequestMessage(NetworkMessage const& msg)
{
    //Step over the two byte header then extract the ID.
    uint32_t potentialOpponentID{0};
    std::memcpy(&potentialOpponentID, msg.data() + 2, sizeof(potentialOpponentID));

    mPotentialOpponentID = ntohl(potentialOpponentID);
    mIsThereAPotentialOpponent = true;

    pubEvent<NetworkEvents::PairRequest>(mPotentialOpponentID);
}

void ConnectionManager::handleMoveMessage(NetworkMessage const& netMsg)
{
    //The size of netMsg is asserted to be correct in processNetworkMessage()

    //The layout of the MOVE_MSGTYPE type of message (class ChessMove defined in move.h client code):
    //|0|1|2|3|4|5|6|
    //byte 0 will be the MOVE_MSGTYPE  <--- header bytes
    //byte 1 will be the MOVE_MSGSIZE  <---
    //
    //byte 2 will be the file (0-7) the piece if moving from   <--- source square
    //byte 3 will be the rank (0-7) the piece if moving from   <---
    // 
    //byte 4 will be the file (0-7) the piece if moving to   <--- destination square
    //byte 5 will be the rank (0-7) the piece if moving to   <---
    // 
    //byte 6 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one
    //byte 7 will be the MoveInfo (enum defined in (client source)moveInfo.h)

    ChessMove const move
    {
        {static_cast<int>(netMsg[2]), static_cast<int>(netMsg[3])}, //source square
        {static_cast<int>(netMsg[4]), static_cast<int>(netMsg[5])}, //destination square
        static_cast<ChessMove::MoveTypes>(netMsg[7]),
        static_cast<ChessMove::PromoTypes>(netMsg[6]),
    };

    pubEvent<NetworkEvents::OpponentMadeMove>(move);
}

bool ConnectionManager::isOpponentIDStringValid(std::string_view opponentID)
{
    //The ID is a uint32_t as a string in base 10. The string referenced by
    //opponentID should not exceed 10 chars. '\0' is not counted by size().
    if(opponentID.size() > 10 || opponentID.size() == 0)
        return false;

    //If any of the chars are not 0-9.
    for(auto const& c : opponentID)
    {
        if( ! std::isdigit(c) )
            return false;
    }

    uint64_t bigID {std::strtoull(opponentID.data(), nullptr, 10)};

    if(bigID > UINT32_MAX)
        return false;

    return true;
}

void ConnectionManager::handleUnpairMessage()
{
    mIsPairedWithOpponent = false;

    assert(mBoardEventSubscriber.unsub<BoardEvents::MoveCompleted>(mMoveCompletedSubID));

    pubEvent<NetworkEvents::Unpair>();
}

template<typename EventT, typename... EventArgs>
void ConnectionManager::pubEvent(EventArgs&&... eventArgs)
{
    EventT evnt{std::forward<EventArgs>(eventArgs)...};
    mNetworkEventPublisher.pub(evnt);
}

void ConnectionManager::buildAndSendMoveMsgType(ChessMove const& move)
{
    //Pack all of the move information into a buffer to be sent over the network.
    std::array<std::byte, static_cast<size_t>(MessageSize::MOVE_MSGSIZE)> msgBuff {};

    //the layout of the MOVE_MSGTYPE type of message: 
    //|0|1|2|3|4|5|6|
    //byte 0 will be the MOVE_MSGTYPE and byte 1 will be the MOVE_MSGSIZE
    //
    //byte 2 will be the file (0-7) the piece if moving from 
    //byte 3 will be the rank (0-7) the piece if moving from 
    // 
    //byte 4 will be the file (0-7) the piece if moving to
    //byte 5 will be the rank (0-7) the piece if moving to
    // 
    //byte 6 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one
    //byte 7 will be the MoveInfo (enum defined in (client source)moveInfo.h)
    msgBuff[0] = static_cast<std::byte>(MessageType::MOVE_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::MOVE_MSGSIZE);
    msgBuff[2] = static_cast<std::byte>(move.src.x);
    msgBuff[3] = static_cast<std::byte>(move.src.y);
    msgBuff[4] = static_cast<std::byte>(move.dest.x);
    msgBuff[5] = static_cast<std::byte>(move.dest.y);
    msgBuff[6] = static_cast<std::byte>(move.promoType);
    msgBuff[7] = static_cast<std::byte>(move.moveType);

    mServerConn.write(msgBuff);
}

void ConnectionManager::buildAndSendPairRequest(uint32_t potentialOpponent)
{
    mPotentialOpponentID = potentialOpponent;
    mIsThereAPotentialOpponent = true;

    potentialOpponent = htonl(potentialOpponent);//Swap to network byte order if necessary.

    std::array<std::byte, static_cast<size_t>(MessageSize::PAIR_REQUEST_MSGSIZE)> msgBuff {};
    msgBuff[0] = static_cast<std::byte>(MessageType::PAIR_REQUEST_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::PAIR_REQUEST_MSGSIZE);
    std::memcpy(msgBuff.data() + 2, &potentialOpponent, sizeof(potentialOpponent));
    
    mServerConn.write(msgBuff);
}

void ConnectionManager::buildAndSendPairAccept()
{
    assert(mIsThereAPotentialOpponent);

    uint32_t opponentID = htonl(mPotentialOpponentID);
    std::array<std::byte, static_cast<size_t>(MessageSize::PAIR_ACCEPT_MSGSIZE)> msgBuff {};
    msgBuff[0] = static_cast<std::byte>(MessageType::PAIR_ACCEPT_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::PAIR_ACCEPT_MSGSIZE);
    std::memcpy(msgBuff.data() + 2, &opponentID, sizeof(opponentID));
    mServerConn.write(msgBuff);
}

void ConnectionManager::buildAndSendPairDecline()
{
    assert(mIsThereAPotentialOpponent);

    uint32_t potentialOpponentID = htonl(mPotentialOpponentID);
    std::array<std::byte, static_cast<size_t>(MessageSize::PAIR_DECLINE_MSGSIZE)> msgBuff {};
    msgBuff[0] = static_cast<std::byte>(MessageType::PAIR_DECLINE_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::PAIR_DECLINE_MSGSIZE);
    std::memcpy(msgBuff.data() + 2, &potentialOpponentID, sizeof(potentialOpponentID));
    mServerConn.write(msgBuff);
}

//A lot of messages have no "payload", but just the two byte header.
void ConnectionManager::sendHeaderOnlyMessage(MessageType msgType)
{
    std::array<std::byte, 2> msgBuff{ static_cast<std::byte>(msgType), std::byte{2} };
    mServerConn.write(msgBuff);
}

void ConnectionManager::handleInvalidMessageType()
{
    FileErrorLogger::get().log("invalid message type sent from the server uh oh...");
}