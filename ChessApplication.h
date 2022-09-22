#pragma once
#include "Vector2i.h"
#include "Board.h"
#include "Window.h"
#include "SDL.h"

#define MAIN_WINDOW_WIDTH 896
#define MAIN_WINDOW_HEIGHT 896

//singleton chessApp that contains the board (which owns/"holds" the pieces)
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

    void run();//main game loop

    inline static ChessApp& getApp(){return s_theApplication;};
    inline static Board& getBoard(){return s_theApplication.m_board;};
    inline static Uint32 getSquareSize(){return s_theApplication.m_squareSize;};
    inline static SDL_Renderer* getCurrentRenderer(){return s_theApplication.m_wnd.m_renderer;};
    inline static int chessPos2Index(Vec2i const pos){return pos.y * 8 + pos.x;};
    inline static Vec2i index2ChessPos(int const index){return{index % 8,index / 8};};
    static Vec2i chess2ScreenPos(Vec2i const);
    static Vec2i screen2ChessPos(Vec2i const);
    static bool inRange(Vec2i const chessPos);//tells if a chess position is on the board
    static bool isMouseOver(SDL_Rect const&);//tells wether mouse position is over a given rectangle
    static void initCircleTexture(int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a, SDL_Texture** toInit);

    inline static SDL_Texture* getCircleTexture(){return s_theApplication.m_circleTexture;};
    inline static SDL_Texture* getRedCircleTexture(){return s_theApplication.m_redCircleTexture;};

    static void promotionRoutine(Vec2i promotionSquare, Side wasPawnWhite);

private:

    static void drawSquares();
    static void drawPieces();
    static void drawIndicatorCircles();

    Window m_wnd;//my simple wrapper class for SDL window
    Uint32 m_squareSize;//square size in pixels
    Board  m_board;//the singleton board composed here as part of the app
    SDL_Texture* m_circleTexture;
    SDL_Texture* m_redCircleTexture;
};