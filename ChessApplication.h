#pragma once
#include "Vector2i.h"
#include "Board.h"
#include "Window.h"
#include "WavSound.h"
#include "SDL.h"
#include "imgui.h"
#include "ChessNetworking.h"

#define NUM_OF_PIECE_TEXTURES 12 //6 types of pieces * 2 for both sides

enum struct Side : Uint32
{
    INVALID, BLACK, WHITE
};

enum struct GameState : Uint32
{
    INVALID,
    GAME_IN_PROGRESS,
    CHECKMATE,
    STALEMATE,
    GAME_ABANDONMENT,//when the connection is lost/opponent closes game
    DRAW_AGREEMENT,
    OPPONENT_RESIGNED,
    YOU_RESIGNED
};

//singleton chessApp that contains the board (which owns/"holds" the pieces)
class ChessApp
{
public:

//indecies into the array of SDL textures for the different pieces.
//scoped to ChessApp but is an unscoped enum to allow for convenient 
//implicit conversion to the underlying type 
//(the textures are in the single chess app instance as m_pieceTextures)
enum TextureIndices : Uint32
{
    WPAWN, WKNIGHT, WROOK, WBISHOP, WQUEEN, WKING,
    BPAWN, BKNIGHT, BROOK, BBISHOP, BQUEEN, BKING, INVALID
};

private:

    ChessApp();//called before main() and thus also inits the board and the pieces before main()
    static ChessApp s_theApplication;

public:

    ChessApp(ChessApp const&)=delete;
    ChessApp(ChessApp&&)=delete;
    ChessApp& operator=(ChessApp const&)=delete;
    ChessApp& operator=(ChessApp&&)=delete;

    ~ChessApp();

    void run();//main application loop

    //getter methods
    inline static ChessApp& getApp(){return s_theApplication;}
    inline static Board& getBoard(){return s_theApplication.m_board;}
    inline Uint32 getSquareSize(){return m_squareSize;}
    inline auto const& getTextures(){return m_pieceTextures;}
    inline SDL_Renderer* getCurrentRenderer(){return m_wnd.m_renderer;}
    inline bool isPromotionWndOpen() const {return m_wnd.m_promotionWindowIsOpen;}
    inline GameState getGameState() const {return m_gameState;}

    //helper static methods
    inline static int chessPos2Index(Vec2i const pos){return pos.y * 8 + pos.x;}
    inline static Vec2i index2ChessPos(int const index){return{index % 8, index / 8};}
    static Vec2i chess2ScreenPos(Vec2i const);
    static Vec2i screen2ChessPos(Vec2i const);
    static bool isScreenPositionOnBoard(Vec2i const&);//tells if a screen pos is on the board or if its off the screen/board or is over an imgui window
    static bool inRange(Vec2i const chessPos);//tells if a chess position is within the range of a chess board (0-7 ranks and a-h files)
    static bool isMouseOver(SDL_Rect const&);//tells wether mouse position is over a given rectangle
    static std::string getCurrentDateAndTime();
    inline static bool isUserConnected(){return s_theApplication.m_netWork.isUserConnected();}

    //play audio methods
    inline void playChessMoveSound(){m_pieceMoveSound.playFullSound();}
    inline void playChessCastleSound(){m_pieceCastleSound.playFullSound();}
    inline void playChessCaptureSound(){m_pieceCaptureSound.playFullSound();}

    //open/close imgui windows/popups on the next iteration of main loop
    inline void openPromotionPopup(){m_wnd.m_promotionWindowIsOpen = true;}
    inline void closePromotionPopup(){m_wnd.m_promotionWindowIsOpen = false;}
    inline void openWinLossDrawPopup()
    {
        m_wnd.m_winLossDrawPopupIsOpen = true; 
        updateWinLossDrawMessage();
    }
    inline void closeWinLossDrawPopup(){m_wnd.m_winLossDrawPopupIsOpen = false;}

    //pt will be defaulted to PromoType::INVALID to indicate no promotion is happening. Otherwise
    //it will hold a value that indicates that a promotion took place and which piece to promote to.
    void sendMove(Move const& move, PromoType pt = PromoType::INVALID);

    //There are lots of messages that just consist of the message type.
    //This method is for telling m_network to send one of those simple message types.
    void send1ByteMessage(ChessConnection::NetMessageType NMT);

    void setGameState(GameState gs) {m_gameState = gs;}

private:

    void checkForDisconnect();

    void renderAllTheThings();

    //methods for drawing stuff called in renderAllTheThings()
    void drawSquares();
    void drawPiecesNotOnMouse();
    void drawPieceOnMouse();
    void drawMoveIndicatorCircles();
    void drawColorEditorWindow();
    void drawConnectionWindow();//returns true if a successful connection was made
    void drawPromotionPopup();
    void drawMenuBar();
    void drawResetButtonErrorPopup();
    void drawNewConnectionPopup();
    void drawWinLossDrawPopup();
    void drawRematchRequestWindow();

    //called inside of openWinLossDrawPopup() 
    //in order to update m_winLossDrawPopupMessage before drawing the win loss draw popup
    void updateWinLossDrawMessage();

    friend void ChessConnection::connect2Server(std::string_view targetIP);
    friend void ChessConnection::waitForClient2Connect(long const, long const);

    void onNewConnection();//called from inside P2PChessConnection::connect2Server and waitForClient2connect once a connection is established
    void processIncomingMessages();//called at the top of ChessApp::run()

    //handle the different kinds of messages that can be sent to/from the other player
    void handleMoveMessage(std::string_view msg);//handle the incomming move message from the opponent
    void handleResignMessage();
    void handleDrawOfferMessage();
    void handleRematchRequestMessage();
    void handleRematchAcceptMessage();
    //there is no handle WHICH_SIDE message. that just happens inline inside of onNewConnection() 

    bool processEvents();
    void initCircleTexture(int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a, SDL_Texture** toInit);
    void loadPieceTexturesFromDisk();

    //looks for a file in the same path as the .exe called squareColorData.txt.
    //if it cant find it, then it will throw an exception.
    void deserializeAndLoadSquareColorData();
    void serializeSquareColorData();

    inline void openRematchRequestWindow()  {m_wnd.m_rematchRequestWindowIsOpen = true;}
    inline void closeRematchRequestWindow() {m_wnd.m_rematchRequestWindowIsOpen = false;}

    GameState m_gameState;
    Uint32 const m_chessBoardWidth;
    Uint32 const m_chessBoardHeight;
    Uint32 m_squareSize;//square size in pixels
    float m_menuBarHeight;
    Window   m_wnd;//my simple wrapper class for SDL window and imgui stuff
    WavSound m_pieceMoveSound;
    WavSound m_pieceCastleSound;
    WavSound m_pieceCaptureSound;
    std::array<Uint32, 4> m_lightSquareColor;
    std::array<Uint32, 4> m_darkSquareColor;
    Board  m_board;//the singleton board composed here as part of the ChessApp instance
    SDL_Texture* m_circleTexture;
    SDL_Texture* m_redCircleTexture;
    float const  m_pieceTextureScale;
    std::array<SDL_Texture*, NUM_OF_PIECE_TEXTURES> m_pieceTextures;//the textures for the chess pieces
    std::array<Vec2i,        NUM_OF_PIECE_TEXTURES> m_pieceTextureSizes;//width and height of the above textures
    ChessConnection m_netWork;
    std::string m_winLossDrawPopupMessage;//message that gets displayed when the game is over
};