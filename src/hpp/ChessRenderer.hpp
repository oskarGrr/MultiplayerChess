#pragma once
#include <array>
#include <cstdint>
#include "SDL.h"
#include "Vector2i.h"
#include "PopupManager.hpp"
#include "ChessEvents.hpp"
#include "TextureManager.hpp"
#include "Window.h"
#include "chessNetworkProtocol.h" //enum Side

class Board;
class ConnectionManager;

class ChessRenderer
{
public:

    ChessRenderer(IncommingNetworkEventSystem&, BoardEventSystem&, GUIEventSystem const&);

    auto static getSquareSize() {return sSquareSize;}

    bool isScreenPositionOnBoard(Vec2i const screenPos) const;
    void renderAllTheThings(Board const& b, ConnectionManager const& cm);
    Vec2i screen2ChessPos(Vec2i const pos) const;

    struct PromotionWindowContext
    {
        //promotingSide is the side (white or black) that is promoting, 
        //not the side of the board the promotion is happening on.
        Side  promotingSide;
        Vec2i promotingSquare;
    };

private:

    void drawPromotionPopup();
    void drawColorEditor();
    void drawConnectionWindow();
    void drawMoveIndicatorCircles(Board const&);
    void drawPieceOnMouse();
    void drawSquares();
    void drawPiecesNotOnMouse(Board const&);
    void drawMenuBar(Board const&, ConnectionManager const&);

    //methods to reduce ctor size
    void initSquareColorData();
    void subToEvents(IncommingNetworkEventSystem&, BoardEventSystem&);
    
    //Takes a chess position, and returns the pixel screen coords
    //of where that is (the middle of the square).
    Vec2i chess2ScreenPos(Vec2i const pos);

private:

    inline static uint32_t sSquareSize   {112U};
    inline static uint32_t sWindowWidth  {sSquareSize * 8};
    inline static uint32_t sWindowHeight {sWindowWidth};

    ImVec2 mMenuBarSize {};
    Window mWindow {static_cast<int>(sWindowWidth), static_cast<int>(sWindowHeight), 
        "Chess", SDL_INIT_VIDEO | SDL_INIT_AUDIO, 0u};

    Side mViewingPerspective {Side::WHITE};

    TextureManager mTextureManager {mWindow.renderer};
    PopupManager mPopupManager;

    GUIEventSystem const& mGuiEventPublisher;

    bool mIsColorEditorWindowOpen {false};
    bool mIsConnectionWindowOpen  {false};
    bool mImguiDemoWindowIsOpen   {false};
    bool mIsPromotionWindowOpen   {false};

    std::array<uint32_t, 4> const mDefaultLightSquareColor {214, 235, 225, 255};
    std::array<uint32_t, 4> const mDefaultDarkSquareColor  {43, 86, 65, 255};
    std::array<uint32_t, 4> mLightSquareColor {mDefaultLightSquareColor};
    std::array<uint32_t, 4> mDarkSquareColor  {mDefaultDarkSquareColor};

    PromotionWindowContext mPromotionContext {Side::INVALID, INVALID_VEC2I};

public:
    ChessRenderer(ChessRenderer const&)=delete;
    ChessRenderer(ChessRenderer&&)=delete;
    ChessRenderer& operator=(ChessRenderer const&)=delete;
    ChessRenderer& operator=(ChessRenderer&&)=delete;
};