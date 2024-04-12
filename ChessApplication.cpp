#include <memory>//std::make_unique/make_shared and std::unique_ptr/shared_ptr

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

ChessApp::~ChessApp()
{
}

void ChessApp::run()
{
    setGameState(GameState::GAME_IN_PROGRESS);
    bool shouldQuit = false;
    while(!shouldQuit)
    {
        processIncomingMessages();
        shouldQuit = processEvents();
        m_chessDrawer.renderAllTheThings();
        SDL_Delay(10);
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

void ChessApp::buildAndSendPairRequest(std::string_view IPAddr)
{
    assert(IPAddr.size() == PAIR_REQUEST_MSG_SIZE);

    in_addr ipOfPotentialOpponent;//ip in network byte order
    inet_pton(AF_INET, IPAddr.data(), &ipOfPotentialOpponent);

    char pairRequestMsg[PAIR_REQUEST_MSG_SIZE] = {0};
    pairRequestMsg[0] = PAIR_REQUEST_MSGTYPE;

    std::memcpy(pairRequestMsg + 1, 
        &ipOfPotentialOpponent, 
        sizeof ipOfPotentialOpponent);

    m_network.sendMessage(pairRequestMsg);
}

//Builds the move message and then passes it to the m_network's sendMessage method.
//pt will be defaulted to PromoType::INVALID to indicate no promotion is happening. Otherwise
//it will hold a value that indicates that a promotion took place and which piece to promote to.
void ChessApp::buildAndSendMoveMsg(Move const& move, PromoType pt)
{
    //pack all of the move information into a string to be sent
    std::string moveMsg;
    moveMsg.resize(MOVE_MSG_SIZE);

    //the layout of the MOVE_MSGTYPE type of message: 
    //|0|1|2|3|4|5|6|
    //byte 0 will be the MOVE_MSGTYPE
    //byte 1 will be the file (0-7) of the square where the piece is that will be moving
    //byte 2 will be the rank (0-7) of the square where the piece is that will be moving
    //byte 3 will be the file (0-7) of the square where the piece will be moving to
    //byte 4 will be the rank (0-7) of the square where the piece will be moving to
    //byte 5 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one
    //byte 6 will be the MoveInfo (enum defined in (client source)moveInfo.h) of the move that is happening
    moveMsg[0] = static_cast<char>(MOVE_MSGTYPE);
    moveMsg[1] = move.m_source.x;
    moveMsg[2] = move.m_source.y;
    moveMsg[3] = move.m_dest.x;
    moveMsg[4] = move.m_dest.y;
    moveMsg[5] = static_cast<char>(pt);
    moveMsg[6] = static_cast<char>(move.m_moveType);

    m_network.sendMessage(moveMsg);
}

void ChessApp::buildAndSendPairAccept()
{
    in_addr nwByteOrderIp = m_network.getIpv4OfPotentialOpponent();
    char pairAcceptMsg[PAIR_ACCEPT_MSG_SIZE] = {0};
    pairAcceptMsg[0] = PAIR_ACCEPT_MSGTYPE;
    std::memcpy(pairAcceptMsg + 1, &nwByteOrderIp, sizeof(nwByteOrderIp));
    m_network.sendMessage(pairAcceptMsg);
}

//1 byte messages only consist of their MSGTYPE (defined in chessAppLevelProtocol.h)
void ChessApp::send1ByteMessage(messageType_t msgType)
{
    char msg[1]{msgType};
    m_network.sendMessage(msg);
}

//handle the incoming move message from the opponent
void ChessApp::handleMoveMessage(std::string_view msg)
{
    assert(msg.size() == MOVE_MSG_SIZE);
    MoveInfo moveType = static_cast<MoveInfo>(msg.at(6));
    Move move = {{msg.at(1), msg.at(2)}, {msg.at(3), msg.at(4)}, moveType};
    PromoType pt = static_cast<PromoType>(msg.at(5));
    m_board.movePiece(move);
    m_board.postMoveUpdate(move, pt);
}

void ChessApp::handleResignMessage()
{
    setGameState(GameState::OPPONENT_RESIGNED);
    m_chessDrawer.openOrCloseWinLossDrawWindow(true);
}

//no draw offer button yet
void ChessApp::handleDrawOfferMessage()
{
}

//the layout of the MOVE_MSGTYPE type of message: 
//|0|1|2|3|4|5|6|
//byte 0 will be the MOVE_MSGTYPE
//byte 1 will be the file (0-7) of the square where the piece is that will be moving
//byte 2 will be the rank (0-7) of the square where the piece is that will be moving
//byte 3 will be the file (0-7) of the square where the piece will be moving to
//byte 4 will be the rank (0-7) of the square where the piece will be moving to
//byte 5 will be the PromoType (enum defined in (client source)moveInfo.h) of the promotion if there is one
//byte 6 will be the MoveInfo (enum defined in (client source)moveInfo.h) of the move that is happening
void ChessApp::handlePairRequestMessage(std::string_view msg)
{
    //step over the first PAIR_REQUEST_MSGTYPE byte
    //(message types defined in chessAppLevelProtocol.h)
    //and set the potential opponent. now the client has
    //PAIR_REQUEST_TIMEOUT_SECS seconds before the request times out
    in_addr nwByteOrderPotentialOpponentIp{};
    std::memcpy(&nwByteOrderPotentialOpponentIp, 
        msg.data()+1, sizeof nwByteOrderPotentialOpponentIp);

    m_network.setPotentialOpponent(nwByteOrderPotentialOpponentIp);
    m_chessDrawer.openOrClosePairRequestWindow(true);
}

void ChessApp::handlePairingCompleteMessage(std::string_view msg)
{
    Side blackOrWhite = static_cast<Side>(msg.at(1));
    m_board.setSideUserIsPlayingAs(blackOrWhite);
    m_board.resetBoard();
    m_board.setBoardViewingPerspective(blackOrWhite);
    m_network.setIsPairedWithOpponent(true);
    m_chessDrawer.openOrCloseConnectionWindow(false);
    m_chessDrawer.openOrClosePairRequestWindow(false);
    m_chessDrawer.openOrCloseNewOpponentWindow(true);
}

void ChessApp::handleRematchRequestMessage()
{
    m_chessDrawer.openOrCloseRematchRequestWindow(true);
    m_chessDrawer.openOrCloseWinLossDrawWindow(false);
}

void ChessApp::handleRematchAcceptMessage()
{
    m_board.resetBoard();
}

void ChessApp::handleOpponentClosedConnectionMessage()
{
    setGameState(GameState::GAME_ABANDONMENT);
    m_network.setIsPairedWithOpponent(false);
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

//called once per frame at the beginning of the frame in ChessApp::run()
void ChessApp::processIncomingMessages()
{
    if(!m_network.isConnectedToServer())
        return;

    if(auto result = m_network.recieveMessageIfAvailable())//std::optional operator bool() called
    {
        auto& msg = result.value();

        //the first byte is the MSGTYPE (defined in chessAppLevelProtocol.h)
        messageType_t msgType = static_cast<messageType_t>(msg.at(0));

        switch(msgType)
        {
        case MOVE_MSGTYPE:             handleMoveMessage(msg);             break;
        case UNPAIR_MSGTPYE:           handleUnpairMessage();              break;
        case RESIGN_MSGTYPE:           handleResignMessage();              break;
        //case DRAW_OFFER_MSGTYPE:     handleDrawOfferMessage();           break;
        case REMATCH_ACCEPT_MSGTYPE:   handleRematchAcceptMessage();       break;
        case REMATCH_REQUEST_MSGTYPE:  handleRematchRequestMessage();      break;
        case PAIR_REQUEST_MSGTYPE:     handlePairRequestMessage(msg);      break;
        case PAIRING_COMPLETE_MSGTYPE: handlePairingCompleteMessage(msg);  break;
        case REMATCH_DECLINE_MSGTYPE:  handleRematchDeclineMessage();      break;
        case OPPONENT_CLOSED_CONNECTION_MSGTPYE: handleOpponentClosedConnectionMessage();
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

//tells wether mouse position is over a given rectangle
bool ChessApp::isMouseOver(SDL_Rect const& r)
{
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    return(x >= r.x && x <= r.x+r.w && y >= r.y && y <= r.h + r.y);
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
    ret.y += static_cast<int>(s_theApplication.m_chessDrawer.getMenuBarHeightInPixels());

    return ret;
}

//wont check if pos is on the chess board callee must do that
Vec2i ChessApp::screen2ChessPos(Vec2i const pos)
{
    auto squareSize = s_theApplication.m_chessDrawer.getSquareSizeInPixels();

    Vec2i ret
    {
        static_cast<int>(pos.x / squareSize),
        static_cast<int>(pos.y / squareSize)
    };

    if(s_theApplication.m_board.getViewingPerspective() == Side::WHITE)
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
bool ChessApp::isScreenPositionOnBoard(Vec2i const& screenPosition)
{
    return !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
}