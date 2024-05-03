#include <memory>//std::make_unique/make_shared and std::unique_ptr/shared_ptr
#include <chrono>

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "SDL.h"
#include "SDL_image.h"
#include "ChessNetworking.h"
#include "errorLogger.h"
#include "ChessDrawer.h"
#include "PieceTypes.h"

ChessApp ChessApp::s_theApplication{};

#define NUM_OF_PIECE_TEXTURES 12 //6 types of pieces * 2 for both sides
#define INITIAL_SQUARESIZE_INPIXELS 112U
#define INITIAL_BOARDWIDTH (INITIAL_SQUARESIZE_INPIXELS * 8)
#define INITIAL_BOARDHEIGHT (INITIAL_SQUARESIZE_INPIXELS * 8)

ChessApp::ChessApp() try :
      m_wnd{INITIAL_BOARDWIDTH, INITIAL_BOARDHEIGHT, "Chess", SDL_INIT_VIDEO | SDL_INIT_AUDIO, 0u}, 
      m_chessDrawer{INITIAL_SQUARESIZE_INPIXELS}, m_gameState{GameState::INVALID},
      m_pieceMoveSound{"sounds/woodChessMove.wav"}, m_pieceCastleSound{"sounds/woodChessCastle.wav"}, 
      m_pieceCaptureSound{"sounds/woodCaptureMove.wav"}, m_network{}, m_board{}
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}
catch(std::exception& e)
{
    std::string errMsg{e.what()};
    errMsg.append(" thrown durring initialization of the static ChessApp before main()");
    FileErrorLogger::get().logErrors(errMsg);
}
catch(...)
{
    FileErrorLogger::get().logErrors("exception of unknown type thrown durring"
        "initialization of the static ChessApp before main()");
}

void ChessApp::run()
{
    updateGameState(GameState::GAME_IN_PROGRESS);
    bool shouldQuit = false;
    double dt{0.0};

    while(!shouldQuit)
    {
        auto start = std::chrono::steady_clock::now();

        processIncomingMessages();
        shouldQuit = processEvents();
        m_chessDrawer.renderAllTheThings();
        SDL_Delay(10);

        auto end = std::chrono::high_resolution_clock::now();
        dt = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1.0E9;
    }
}

//return true if we should close the app
bool ChessApp::processEvents()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        //allow imgui to process its own events and update the IO state
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch(event.type)
        {
        case SDL_QUIT: return true;
        case SDL_MOUSEBUTTONDOWN: m_board.piecePickUpRoutine(event); break;
        case SDL_MOUSEBUTTONUP:   m_board.piecePutDownRoutine(event);
        }
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    
    return false;
}

//Upon connecting to the server, the client receives a unique uint32_t ID.
//That ID can then be input to specify to the server what player you
//want to play against like a "friend code". opponentID param is this ID.
//This function will NOT do a check to make sure the string referenced by opponentID is a valid ID string.
void ChessApp::buildAndSendPairRequest(std::string_view opponentID)
{
    //string to ___ functions really should be updated to take a 
    //string_view instead of string const& :(
    uint32_t ID = std::stoul(static_cast<std::string>(opponentID));

    m_network.setPotentialOpponentID(ID);
    m_network.setIsThereAPotentialOpponent(true);

    ID = htonl(ID);

    std::byte msgBuff[static_cast<unsigned>(MessageSize::PAIR_REQUEST_MSGSIZE)]{};
    msgBuff[0] = static_cast<std::byte>(MessageType::PAIR_REQUEST_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::PAIR_REQUEST_MSGSIZE);
    std::memcpy(msgBuff + 2, &ID, sizeof(ID));

    m_network.sendMessage(reinterpret_cast<char*>(msgBuff), sizeof(msgBuff));
}

//Builds the move message and then passes it to the m_network's sendMessage method.
//pt will be defaulted to PromoType::INVALID to indicate no promotion is happening. Otherwise
//it will hold a value that indicates that a promotion took place and which piece to promote to.
void ChessApp::buildAndSendMoveMsg(Move const& move, PromoType pt)
{
    //Pack all of the move information into a buffer to be sent over the network.
    std::byte msgBuff[static_cast<unsigned>(MessageSize::MOVE_MSGSIZE)]{};

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
    msgBuff[2] = static_cast<std::byte>(move.m_source.x);
    msgBuff[3] = static_cast<std::byte>(move.m_source.y);
    msgBuff[4] = static_cast<std::byte>(move.m_dest.x);
    msgBuff[5] = static_cast<std::byte>(move.m_dest.y);
    msgBuff[6] = static_cast<std::byte>(pt);
    msgBuff[7] = static_cast<std::byte>(move.m_moveType);

    m_network.sendMessage(reinterpret_cast<char*>(msgBuff), sizeof(msgBuff));
}

void ChessApp::buildAndSendPairAccept()
{
    uint32_t opponentID = htonl(m_network.getPotentialOpponentsID());
    std::byte msgBuff[static_cast<unsigned>(MessageSize::PAIR_ACCEPT_MSGSIZE)]{};
    msgBuff[0] = static_cast<std::byte>(MessageType::PAIR_ACCEPT_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::PAIR_ACCEPT_MSGSIZE);
    std::memcpy(msgBuff + 2, &opponentID, sizeof(opponentID));
    m_network.sendMessage(reinterpret_cast<char*>(msgBuff), sizeof(msgBuff));
}

void ChessApp::buildAndSendPairDecline()
{
    assert(m_network.isThereAPotentialOpponent());

    uint32_t potentialOpponentID = htonl(m_network.getPotentialOpponentsID());
    std::byte msgBuff[static_cast<unsigned>(MessageSize::PAIR_DECLINE_MSGSIZE)]{};
    msgBuff[0] = static_cast<std::byte>(MessageType::PAIR_DECLINE_MSGTYPE);
    msgBuff[1] = static_cast<std::byte>(MessageSize::PAIR_DECLINE_MSGSIZE);
    std::memcpy(msgBuff + 2, &potentialOpponentID, sizeof(potentialOpponentID));

    m_network.sendMessage(reinterpret_cast<char*>(msgBuff), sizeof(msgBuff));
}

//A lot of messages have no "payload" but just the two byte header.
void ChessApp::sendHeaderOnlyMessage(MessageType msgType, MessageSize msgSize)
{
    std::byte msgHeaderBuff[2] = {static_cast<std::byte>(msgType), 
        static_cast<std::byte>(msgSize)};

    m_network.sendMessage(reinterpret_cast<char*>(msgHeaderBuff), 
        sizeof(msgHeaderBuff));
}

//The layout of the MOVE_MSGTYPE type of message:
//|0|1|2|3|4|5|6|7|
//Byte 0 will be MessageType::MOVE_MSGTYPE.
//Byte 1 will be MessageSize::MOVE_MSGSIZE.
//--------------------------------------------------------------------------------------
//Byte 2 will be the file (0-7) of the square where the piece is moving from.
//Byte 3 will be the rank (0-7) of the square where the piece is moving from.
//Byte 4 will be the file (0-7) of the square where the piece is moving to.
//Byte 5 will be the rank (0-7) of the square where the piece is moving to.
//Byte 6 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one.
//Byte 7 will be the MoveInfo (enum defined in (client source)moveInfo.h) of the move that is happening.
void ChessApp::handleMoveMessage(std::vector<char> const& msg)
{
    assert(msg.size() == static_cast<std::size_t>(MessageSize::MOVE_MSGSIZE));

    auto moveType = static_cast<MoveInfo>(msg.at(7));
    Move move = {{msg.at(2), msg.at(3)}, {msg.at(4), msg.at(5)}, moveType};
    PromoType pt = static_cast<PromoType>(msg.at(6));
    m_board.movePiece(move);
    m_board.postMoveUpdate(move, pt);
}

void ChessApp::handleResignMessage()
{
    updateGameState(GameState::OPPONENT_RESIGNED);
    m_chessDrawer.openOrCloseWinLossDrawWindow(true);
}

void ChessApp::handleDrawOfferMessage()
{
    m_chessDrawer.openOrCloseDrawOfferWindow(OPEN_WINDOW);
}

void ChessApp::hanldeDrawDeclineMessage()
{
    m_chessDrawer.openOrCloseDrawDeclinedWindow(OPEN_WINDOW);
}

void ChessApp::handlePairRequestMessage(std::vector<char> const& msg)
{
    assert(msg.size() == static_cast<std::size_t>(MessageSize::PAIR_REQUEST_MSGSIZE));

    //Step over the two byte header then extract the ID.
    uint32_t potentialOpponentID{0};
    std::memcpy(&potentialOpponentID, msg.data() + 2, sizeof(potentialOpponentID));

    m_network.setPotentialOpponentID(ntohl(potentialOpponentID));
    m_network.setIsThereAPotentialOpponent(true);
    m_chessDrawer.openOrClosePairRequestWindow(true);
}

void ChessApp::handlePairingCompleteMessage(std::vector<char> const& msg)
{
    assert(msg.size() == static_cast<std::size_t>(MessageSize::PAIR_COMPLETE_MSGSIZE));

    Side blackOrWhite = static_cast<Side>(msg.at(2));
    m_board.setSideUserIsPlayingAs(blackOrWhite);
    m_board.resetBoard();
    m_board.setBoardViewingPerspective(blackOrWhite);
    m_network.setIsPairedWithOpponent(true);
    m_network.setIsThereAPotentialOpponent(false);
    m_network.setOpponentID(m_network.getPotentialOpponentsID());
    m_chessDrawer.openOrCloseConnectionWindow(CLOSE_WINDOW);
    m_chessDrawer.openOrClosePairingCompleteWindow(OPEN_WINDOW);
}

void ChessApp::handleRematchRequestMessage()
{
    m_chessDrawer.openOrCloseRematchRequestWindow(OPEN_WINDOW);
    m_chessDrawer.openOrCloseWinLossDrawWindow(CLOSE_WINDOW);
}

void ChessApp::handleRematchAcceptMessage()
{
    m_board.resetBoard();
}

void ChessApp::handleOpponentClosedConnectionMessage()
{
    m_network.setIsPairedWithOpponent(false);
    updateGameState(GameState::GAME_ABANDONMENT);
    m_chessDrawer.openOrCloseWinLossDrawWindow(true);
}

void ChessApp::handleRematchDeclineMessage()
{
    m_network.setIsPairedWithOpponent(false);
    m_board.resetBoard();
}

void ChessApp::handleUnpairMessage()
{
    m_network.setIsPairedWithOpponent(false);
    m_board.resetBoard();
}

//Handle the NEW_ID_MSGTYPE. This message is sent from the server
//immediately uppon connection. The message is meant to give the client
//their unique ID to use as a "friend code". Other players can then
//give this friend code to the server to specify who they want to play chess against.
//This is a big improvment over just simply giving your opponents external IP address.
void ChessApp::handleNewIDMessage(std::vector<char> const& msg)
{
    assert(msg.size() == static_cast<std::size_t>(MessageSize::NEW_ID_MSGSIZE));
    uint32_t newID{0};
    std::memcpy(&newID, msg.data() + 2, sizeof(newID));
    m_network.setUniqueID(ntohl(newID));
}

//handle ID_NOT_IN_LOBBY_MSGTYPE
void ChessApp::handleIDNotInLobbyMessage()
{
    assert(m_network.isThereAPotentialOpponent());
    m_chessDrawer.openOrCloseIDNotInLobbyWindow(OPEN_WINDOW);
}

void ChessApp::handleDrawAcceptMessage()
{
    updateGameState(GameState::DRAW_AGREEMENT);
    m_chessDrawer.openOrCloseWinLossDrawWindow(OPEN_WINDOW);
}

void ChessApp::handlePairDeclineMessage(std::vector<char> const& msg)
{
    assert(msg.size() == static_cast<std::size_t>(MessageSize::PAIR_DECLINE_MSGSIZE));

    //The ID of the player that declined our PAIR_ACCEPT_MSGTYPE.
    uint32_t potentialOpponentID{0};
    std::memcpy(&potentialOpponentID, msg.data() + 2, sizeof(potentialOpponentID));
    potentialOpponentID = ntohl(potentialOpponentID);
    assert(m_network.getPotentialOpponentsID() == potentialOpponentID);
    m_chessDrawer.openOrClosePairDeclineWindow(OPEN_WINDOW);
    m_network.setIsThereAPotentialOpponent(false);
}

void ChessApp::handleInvalidMessageType()
{
    FileErrorLogger::get().logErrors("invalid message type sent from the server uh oh...");
}

//called once per frame at the beginning of the frame in ChessApp::run()
void ChessApp::processIncomingMessages()
{
    if(!m_network.isConnectedToServer())
        return;

    if(auto result = m_network.recieveMessageIfAvailable())//std::optional operator bool() called
    {
        auto& msg = result.value();
        auto const msgType = static_cast<MessageType>(msg.at(0));

        switch(msgType)
        {
        using enum MessageType;//only using this enum's namespace inside this switch
        case MOVE_MSGTYPE:             handleMoveMessage(msg);             break;
        case ID_NOT_IN_LOBBY_MSGTYPE:  handleIDNotInLobbyMessage();        break;
        case UNPAIR_MSGTYPE:           handleUnpairMessage();              break;
        case RESIGN_MSGTYPE:           handleResignMessage();              break;
        case DRAW_OFFER_MSGTYPE:       handleDrawOfferMessage();           break;
        case DRAW_DECLINE_MSGTYPE:     hanldeDrawDeclineMessage();         break;
        case DRAW_ACCEPT_MSGTYPE:      handleDrawAcceptMessage();          break;
        case REMATCH_ACCEPT_MSGTYPE:   handleRematchAcceptMessage();       break;
        case REMATCH_REQUEST_MSGTYPE:  handleRematchRequestMessage();      break;
        case PAIR_REQUEST_MSGTYPE:     handlePairRequestMessage(msg);      break;
        case PAIRING_COMPLETE_MSGTYPE: handlePairingCompleteMessage(msg);  break;
        case REMATCH_DECLINE_MSGTYPE:  handleRematchDeclineMessage();      break;
        case NEW_ID_MSGTYPE:           handleNewIDMessage(msg);            break;
        case PAIR_DECLINE_MSGTYPE:     handlePairDeclineMessage(msg);      break;
        case OPPONENT_CLOSED_CONNECTION_MSGTYPE: handleOpponentClosedConnectionMessage(); break;
        default: handleInvalidMessageType();
        }
    }
    
    //if we are no longer connected as indicated by recv()
    //returning 0 inside m_network.recieveMessageIfAvailable()
    if(!m_network.isConnectedToServer())
    {
        m_board.resetBoard();
        m_board.setBoardViewingPerspective(Side::WHITE);
    }
}

//tells if a chess position is on the board or not
bool ChessApp::inRange(Vec2i const chessPos)
{
    return chessPos.x <= 7 && chessPos.x >= 0 && chessPos.y <= 7 && chessPos.y >= 0;
}

//take a chess position and flip it to the correct screen position
//based on which perspective the player is viewing the board from
Vec2i ChessApp::chess2ScreenPos(Vec2i const pos)
{
    Vec2i ret{pos};

    if(s_theApplication.m_board.getViewingPerspective() == Side::WHITE)
    {
        ret.y = 7 - ret.y;
    }
    else//if viewing the board from blacks perspective
    {
        ret.x = 7 - ret.x;
    }

    auto squareSize = s_theApplication.m_chessDrawer.getSquareSizeInPixels();

    //scale up to board size
    ret.x *= squareSize;
    ret.y *= squareSize;

    //move from top left to middle of square
    ret.y += static_cast<int>(squareSize * 0.5f);
    ret.x += static_cast<int>(squareSize * 0.5f);

    //move down to account for the menu bar.
    //will only work after the first frame since the first imgui frame
    //needs to be started to measure the menu bar
    ret.y += static_cast<int>(s_theApplication.m_chessDrawer.getMenuBarSize().y);

    return ret;
}

//Will not check if pos is on the chess board.
Vec2i ChessApp::screen2ChessPos(Vec2i const pos) const
{
    auto const squareSize = m_chessDrawer.getSquareSizeInPixels();
    auto const menuBarHeight = static_cast<int>(m_chessDrawer.getMenuBarSize().y);

    Vec2i ret
    {
        static_cast<int>(pos.x / squareSize),
        static_cast<int>((pos.y - menuBarHeight) / squareSize)
    };

    if(m_board.getViewingPerspective() == Side::WHITE)
    {
        ret.y = 7 - ret.y;
    }
    else
    {
        ret.x = 7 - ret.x;
    }

    return ret;
}

//return if the position is on the board (false if over an imgui window)
//in the future I will add a border with the rank file indicators around the board
//and this func will also return false when the mouse is over that border.
bool ChessApp::isScreenPositionOnBoard(Vec2i const& screenPosition) const
{
    return m_chessDrawer.isScreenPositionOnBoard(screenPosition);
}

void ChessApp::updateGameState(GameState const gs)
{
    m_gameState = gs;

    if(gs != GameState::GAME_IN_PROGRESS)
        m_chessDrawer.updateWinLossDrawMessage();
}