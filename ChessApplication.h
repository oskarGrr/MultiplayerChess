#pragma once

#include "ChessDrawer.h"
#include "Vector2i.h"
#include "Board.h"
#include "Window.h"
#include "WavSound.h"
#include "SDL.h"
#include "imgui.h"
#include "ChessNetworking.h"

enum struct GameState : uint32_t
{
    INVALID,
    GAME_IN_PROGRESS,
    CHECKMATE,
    STALEMATE,
    GAME_ABANDONMENT,//when the connection is lost/opponent closes their game
    DRAW_AGREEMENT,
    OPPONENT_RESIGNED,
    YOU_RESIGNED
};

class ChessApp
{
private:

    ChessApp();
    static ChessApp s_theApplication;

public:

    ChessApp(ChessApp const&)=delete;
    ChessApp(ChessApp&&)=delete;
    ChessApp& operator=(ChessApp const&)=delete;
    ChessApp& operator=(ChessApp&&)=delete;

    ~ChessApp();

    void run();//main application loop

    //getter methods
    static auto& getApp(){return s_theApplication;}
    static auto& getBoard() {return s_theApplication.m_board;}
    static auto& getNetWork() {return s_theApplication.m_network;}
    SDL_Renderer* getCurrentRenderer() const {return m_wnd.m_renderer;}
    GameState getGameState() const {return m_gameState;}
    auto getWindowWidth() const {return m_wnd.m_width;}
    auto getWindowHeight() const {return m_wnd.m_height;}
    auto getSDLWindow() {return m_wnd.m_window;}

    //helper methods
    static int chessPos2Index(Vec2i const pos){return pos.y * 8 + pos.x;}
    static Vec2i index2ChessPos(int const index){return{index % 8, index / 8};}
    static Vec2i chess2ScreenPos(Vec2i const);//converts a chess position (0-7 for rank and file) to screen position
    static Vec2i screen2ChessPos(Vec2i const);//converts a screen position to a chess position (0-7 for rank and file)
    static bool isScreenPositionOnBoard(Vec2i const&);//tells if a screen pos is on the board or if its off the screen/board or is over an imgui window
    static bool inRange(Vec2i const chessPos);//tells if a chess position is within the range of a chess board (0-7 ranks and a-h files)
    static bool isMouseOver(SDL_Rect const&);//tells wether mouse position is over a given rectangle
    bool isPairedWithOpponent() const {return m_network.isPairedWithOpponent();}
    bool isConnectedToServer() const {return m_network.isConnectedToServer();}
    bool isPromotionWindowOpen() const {return m_chessDrawer.isPromotionWindowOpen();}
    void openPromotionWindow() {m_chessDrawer.openOrClosePromotionWindow(OPEN_WINDOW);}
    void openWinLossDrawWindow() {m_chessDrawer.openOrCloseWinLossDrawWindow(OPEN_WINDOW);}
    void closeWinLossDrawWindow() {m_chessDrawer.openOrCloseWinLossDrawWindow(CLOSE_WINDOW);}

    //play audio methods
    void playChessMoveSound()    {m_pieceMoveSound.playFullSound();}
    void playChessCastleSound()  {m_pieceCastleSound.playFullSound();}
    void playChessCaptureSound() {m_pieceCaptureSound.playFullSound();}

    void updateGameState(GameState);

    //pt will be defaulted to PromoType::INVALID to indicate no promotion is happening. Otherwise
    //it will hold a value that indicates that a promotion took place and which piece to promote to.
    void buildAndSendMoveMsg(Move const& move, PromoType pt = PromoType::INVALID);
    
    //Upon connecting to the server, the client receives a unique uint32_t ID.
    //That ID can then be input to specify to the server what player you
    //want to play against like a "friend code". opponentID param is this ID.
    //This function will NOT do a check to make sure the string referenced by opponentID is a valid ID string.
    void buildAndSendPairRequest(std::string_view potentialOpponent);
    void buildAndSendPairAccept();

    //1 byte messages only consist of their MSGTYPE (defined in chessAppLevelProtocol.h)
    void send1ByteMessage(messageType_t msgType);

private:

    void processIncomingMessages();//called at the top of ChessApp::run()

    //handle the different kinds of messages that can be sent to/from the other player
    void handleMoveMessage(std::vector<char> const& msg);//handle the incoming move message from the opponent
    void handleResignMessage();
    void handleDrawOfferMessage();
    void hanldeDrawDeclineMessage();
    void handleRematchRequestMessage();
    void handleRematchAcceptMessage();
    void handleRematchDeclineMessage();
    void handlePairingCompleteMessage(std::vector<char> const& msg);
    void handlePairRequestMessage(std::vector<char> const& msg);
    void handleOpponentClosedConnectionMessage();
    void handleUnpairMessage();
    void handleNewIDMessage(std::vector<char> const& msg);
    void handleIDNotInLobbyMessage();
    void handleDrawAcceptMessage();

    bool processEvents();

private:
    Window m_wnd;
    ChessDrawer m_chessDrawer;
    GameState m_gameState;
    WavSound m_pieceMoveSound;
    WavSound m_pieceCastleSound;
    WavSound m_pieceCaptureSound;
    ChessConnection m_network;
    Board m_board;
};