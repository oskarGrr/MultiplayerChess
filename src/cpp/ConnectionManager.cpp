#include "ConnectionManager.hpp"
#include "chessNetworkProtocol.h"
#include "errorLogger.h"
#include "ChessMove.hpp"
#include <cassert>
#include <optional>
#include <array>
#include <utility>

ConnectionManager::ConnectionManager(IncommingNetworkEventSystem const& networkInEventSys, 
    GUIEventSystem& networkOutEventSys, BoardEventSystem& internalEventSys) 
        : mNetworkIncommingPublisher{networkInEventSys}
{
    subToEvents(networkOutEventSys, internalEventSys);
}

void ConnectionManager::subToEvents(GUIEventSystem& networkOutEventSys, 
    BoardEventSystem& internalEventSys)
{
    internalEventSys.sub<BoardEvents::MoveCompleted>([this](Event& e)
    {
        auto const& evnt {static_cast<BoardEvents::MoveCompleted&>(e)};
        buildAndSendMoveMsgType(evnt.move);
    });

    networkOutEventSys.sub<GUIEvents::PairRequest>([this](Event& e)
    {
        auto const& evnt {static_cast<GUIEvents::PairRequest&>(e)};
        buildAndSendPairRequest(evnt.opponentID);
    });

    networkOutEventSys.sub<GUIEvents::PairAccept>([this](Event& e)
    {
        buildAndSendPairAccept();
    });
}

//Call once per main loop iteration.
void ConnectionManager::update()
{
    mServerConn.update();

    if( ! mServerConn.isConnected() )
    {

    }

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

        ret.push_back(std::move(*message));//message variable no longer valid

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

    pubEvent<IncommingNetworkEvents::PairDecline>();

    mIsThereAPotentialOpponent = false;
}

void ConnectionManager::handleIDNotInLobbyMessage(NetworkMessage const& msg)
{
    uint32_t invalidID{0};
    std::memcpy(&invalidID, msg.data() + 2, sizeof(invalidID));

    mIsThereAPotentialOpponent = false;
    
    pubEvent<IncommingNetworkEvents::IDNotInLobby>(ntohl(invalidID));
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
    case ID_NOT_IN_LOBBY_MSGTYPE:  pubEvent<IncommingNetworkEvents::IDNotInLobby>();         break;
    case UNPAIR_MSGTYPE:           handleUnpairMessage();                             break;
    case RESIGN_MSGTYPE:           pubEvent<IncommingNetworkEvents::OpponentHasResigned>();  break;
    case DRAW_OFFER_MSGTYPE:       pubEvent<IncommingNetworkEvents::DrawOffer>();            break;
    case DRAW_DECLINE_MSGTYPE:     pubEvent<IncommingNetworkEvents::DrawDeclined>();         break;
    case DRAW_ACCEPT_MSGTYPE:      pubEvent<IncommingNetworkEvents::DrawAccept>();           break;
    case REMATCH_ACCEPT_MSGTYPE:   pubEvent<IncommingNetworkEvents::RematchAccept>();        break;
    case REMATCH_REQUEST_MSGTYPE:  pubEvent<IncommingNetworkEvents::RematchRequest>();       break;
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
    pubEvent<IncommingNetworkEvents::OpponentClosedConnection>();
}

void ConnectionManager::handleRematchDeclineMessage()
{
    mIsPairedWithOpponent = false;
    pubEvent<IncommingNetworkEvents::RematchDecline>();
}

void ConnectionManager::handlePairingCompleteMessage(NetworkMessage const& msg)
{
    auto const side { static_cast<Side>(msg[2]) };

    mIsPairedWithOpponent = true;
    mIsThereAPotentialOpponent = false;

    mOpponentID = mPotentialOpponentID;

    pubEvent<IncommingNetworkEvents::PairingComplete>(mOpponentID, side);
}

void ConnectionManager::handlePairRequestMessage(NetworkMessage const& msg)
{
    //Step over the two byte header then extract the ID.
    uint32_t potentialOpponentID{0};
    std::memcpy(&potentialOpponentID, msg.data() + 2, sizeof(potentialOpponentID));

    mPotentialOpponentID = ntohl(potentialOpponentID);
    mIsThereAPotentialOpponent = true;

    pubEvent<IncommingNetworkEvents::PairRequest>(mPotentialOpponentID);
}

void ConnectionManager::handleMoveMessage(NetworkMessage const& netMsg)
{
    //The size of netMsg is asserted to be correct in processNetworkMessage()

    //The layout of the MessageType::MOVE_MSGTYPE type of message (class ChessMove defined in move.h client code):
    //|0|1|2|3|4|5|6|7|8|
    //Byte 0 will be MessageType::MOVE_MSGTYPE.
    //Byte 1 will be MessageSize::MOVE_MSGSIZE.
    //--------------------------------------------------------------------------------------
    //Byte 2 will be the file (0-7) of the square where the piece is moving from.
    //Byte 3 will be the rank (0-7) of the square where the piece is moving from.
    //Byte 4 will be the file (0-7) of the square where the piece is moving to.
    //Byte 5 will be the rank (0-7) of the square where the piece is moving to.
    //Byte 6 will be the ChessMove::PromoTypes (enum defined in (client source)moveInfo.h) of the promotion if there was one.
    //Byte 7 will be the ChessMove::MoveTypes (enum defined in (client source)moveInfo.h) of the move that is happening.
    //Byte 8 will be the bool which signifies whether or not the move was a capture of an enemy piece (true means it was a capture)

    ChessMove const move
    {
        {static_cast<int>(netMsg[2]), static_cast<int>(netMsg[3])}, //source square
        {static_cast<int>(netMsg[4]), static_cast<int>(netMsg[5])}, //destination square
        static_cast<ChessMove::MoveTypes>(netMsg[6]),
        static_cast<bool>(netMsg[8]), //bool ChessMove::wasACapture (true means it was a capture of an enemy piece) 
        static_cast<ChessMove::PromoTypes>(netMsg[7])
    };

    pubEvent<IncommingNetworkEvents::OpponentMadeMove>(move);
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
    pubEvent<IncommingNetworkEvents::Unpair>();
}

template<typename EventT, typename... EventArgs>
void ConnectionManager::pubEvent(EventArgs&&... eventArgs)
{
    //The empty brace (for the Event base class) is necessary here for aggregate list initialization to work.
    EventT evnt{ {}, std::forward<EventArgs>(eventArgs)...};
    mNetworkIncommingPublisher.pub(evnt);
}

void ConnectionManager::buildAndSendMoveMsgType(ChessMove const& move)
{
    //Pack all of the move information into a buffer to be sent over the network.
    std::array<std::byte, static_cast<size_t>(MessageSize::MOVE_MSGSIZE)> msgBuff {};

    //the layout of the MOVE_MSGTYPE type of message: 
    //|0|1|2|3|4|5|6|
    //byte 0 will be the MOVE_MSGTYPE
    //byte 1 will be the file (0-7) of the square where the piece is that will be moving
    //byte 2 will be the rank (0-7) of the square where the piece is that will be moving
    //byte 3 will be the file (0-7) of the square where the piece will be moving to
    //byte 4 will be the rank (0-7) of the square where the piece will be moving to
    //byte 5 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one
    //byte 6 will be the MoveInfo (enum defined in (client source)moveInfo.h) of the move that is happening
    msgBuff[0] = static_cast<std::byte>(MessageType::MOVE_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::MOVE_MSGSIZE);
    msgBuff[2] = static_cast<std::byte>(move.src.x);
    msgBuff[3] = static_cast<std::byte>(move.src.y);
    msgBuff[4] = static_cast<std::byte>(move.dest.x);
    msgBuff[5] = static_cast<std::byte>(move.dest.y);
    msgBuff[6] = static_cast<std::byte>(move.promoType);
    msgBuff[7] = static_cast<std::byte>(move.moveType);
    msgBuff[8] = static_cast<std::byte>(move.wasACapture);

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
void ConnectionManager::sendHeaderOnlyMessage(MessageType msgType, MessageSize msgSize)
{
    std::array<std::byte, 2> msgBuff{static_cast<std::byte>(msgType), 
        static_cast<std::byte>(msgSize)};

    mServerConn.write(msgBuff);
}

void ConnectionManager::handleInvalidMessageType()
{
    FileErrorLogger::get().log("invalid message type sent from the server uh oh...");
}