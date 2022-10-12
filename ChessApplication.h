#pragma once
#include "Vector2i.h"
#include "Board.h"
#include "Window.h"
#include "SDL.h"
#include "imgui.h"

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
    static void initCircleTexture(int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a, SDL_Texture** toInit);
    static void promotionRoutine(Vec2i promotionSquare, Side wasPawnWhite);

    inline static SDL_Texture* getCircleTexture(){return s_theApplication.m_circleTexture;};
    inline static SDL_Texture* getRedCircleTexture(){return s_theApplication.m_redCircleTexture;};

private:

    void flipBoard();
    static bool processEvents();
    static void renderAllTheThings();
    static void drawSquares();
    static void drawPieces();
    static void drawIndicatorCircles();

    Uint32 const m_chessBoardWidth;
    Uint32 const m_chessBoardHeight;
    float m_menuBarHeight;
    Uint32 m_squareSize;//square size in pixels
    std::array<float, 4> m_lightSquareColor;
    std::array<float, 4> m_darkSquareColor;
    Window m_wnd;//my simple wrapper class for SDL window
    Board  m_board;//the singleton board composed here as part of the ChessApp instance
    SDL_Texture* m_circleTexture;
    SDL_Texture* m_redCircleTexture;
};