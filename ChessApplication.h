#pragma once
#include "Vector2i.h"
#include "Board.h"
#include "Window.h"
#include "WavSound.h"
#include "SDL.h"
#include "imgui.h"

#define NUM_OF_PIECE_TEXTURES 12 //6 types of pieces * 2 for both sides

//indecies into the array of SDL textures for the different pieces
enum struct TextureIndices : Uint32
{
    WPAWN, WKNIGHT, WROOK, WBISHOP, WQUEEN, WKING,
    BPAWN, BKNIGHT, BROOK, BBISHOP, BQUEEN, BKING, INVALID
};

//singleton chessApp that contains the board (which owns/"holds" the pieces)
class ChessApp
{
private:

    ChessApp();//called before main() and thus also inits the board and the pieces before main()
    static ChessApp s_theApplication;   

public:

    ChessApp(ChessApp const&)=delete;
    ChessApp(ChessApp&&)=delete;
    ChessApp& operator=(ChessApp const&)=delete;
    ChessApp& operator=(ChessApp&&)=delete;
    ~ChessApp();

    void run();//main game loop

    inline static ChessApp& getApp(){return s_theApplication;};
    inline static Board& getBoard(){return s_theApplication.m_board;};
    inline static Uint32 getSquareSize(){return s_theApplication.m_squareSize;};
    inline static SDL_Renderer* getCurrentRenderer(){return s_theApplication.m_wnd.m_renderer;};
    inline static int chessPos2Index(Vec2i const pos){return pos.y * 8 + pos.x;};
    inline static Vec2i index2ChessPos(int const index){return{index % 8,index / 8};};
    static Vec2i chess2ScreenPos(Vec2i const);
    static Vec2i screen2ChessPos(Vec2i const);
    static bool isPositionOnBoard(Vec2i const);
    static bool inRange(Vec2i const chessPos);//tells if a chess position is on the board
    static bool isMouseOver(SDL_Rect const&);//tells wether mouse position is over a given rectangle
    static void playChessMoveSound();

    void promotionRoutine(Vec2i promotionSquare, Side wasPawnWhite);
    inline auto const& getTextures() const {return m_pieceTextures;}

private:

    void flipBoard();
    bool processEvents();
    void renderAllTheThings();
    void drawSquares();
    void drawPieces();
    void drawIndicatorCircles();
    void initCircleTexture(int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a, SDL_Texture** toInit);
    void loadPieceTexturesFromDisk();

    Uint32 const m_chessBoardWidth;
    Uint32 const m_chessBoardHeight;
    Uint32 m_squareSize;//square size in pixels
    float  m_menuBarHeight;

    Window   m_wnd;//my simple wrapper class for SDL window
    WavSound m_pieceMoveSound;

    std::array<Uint8, 4> m_lightSquareColor;
    std::array<Uint8, 4> m_darkSquareColor;

    Board  m_board;//the singleton board composed here as part of the ChessApp instance
    SDL_Texture* m_circleTexture;
    SDL_Texture* m_redCircleTexture;
    float const m_pieceTextureScale;
    std::array<SDL_Texture*, NUM_OF_PIECE_TEXTURES> m_pieceTextures;//the textures for the chess pieces
    std::array<Vec2i,        NUM_OF_PIECE_TEXTURES> m_pieceTextureSizes;//width and height of the above textures
};