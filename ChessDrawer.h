#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "SDL.h"
#include "Vector2i.h"

//this class is for drawing the necessary stuff
//with SDL and imgui for the chess application like
//the squares pieces and imgui windows etc

#define NUMOF_PIECE_TEXTURES 12
#define CLOSE_WINDOW false
#define OPEN_WINDOW true

class ChessDrawer
{
public:

    ChessDrawer(uint32_t squareSize);
    ~ChessDrawer();

    ChessDrawer(ChessDrawer const&) = delete;
    ChessDrawer(ChessDrawer&&) = delete;
    ChessDrawer& operator=(ChessDrawer const&) = delete;
    ChessDrawer& operator=(ChessDrawer&&) = delete;

    //Update m_winLossDrawPopupMessage based on the current gamestate.
    void updateWinLossDrawMessage();

    //true for open false for close or use the more readable macros CLOSE_WINDOW and OPEN_WINDOW
    void openOrCloseColorEditorWindow     (bool openOrCLose) {m_colorEditorWindowIsOpen     = openOrCLose;}
    void openOrCloseDemoWindow            (bool openOrCLose) {m_demoWindowIsOpen            = openOrCLose;}
    void openOrClosePromotionWindow       (bool openOrCLose) {m_promotionWindowIsOpen       = openOrCLose;}
    void openOrCloseConnectionWindow      (bool openOrCLose) {m_connectionWindowIsOpen      = openOrCLose;}
    void openOrCloseResetBoardWindow      (bool openOrCLose) {m_resetBoardWindowIsOpen      = openOrCLose;}
    void openOrClosePairingCompleteWindow (bool openOrCLose) {m_pairingCompleteWindowIsOpen = openOrCLose;}
    void openOrCloseWinLossDrawWindow     (bool openOrCLose) {m_winLossDrawWindowIsOpen     = openOrCLose;}
    void openOrCloseRematchRequestWindow  (bool openOrCLose) {m_rematchRequestWindowIsOpen  = openOrCLose;}
    void openOrClosePairRequestWindow     (bool openOrCLose) {m_pairRequestWindowIsOpen     = openOrCLose;}
    void openOrCloseIDNotInLobbyWindow    (bool openOrClose) {m_IDNotInLobbyWindowIsOpen    = openOrClose;}
    void openOrCloseDrawOfferWindow       (bool openOrClose) {m_drawOfferWindowIsOpen       = openOrClose;}
    void openOrCloseDrawDeclinedWindow    (bool openOrClose) {m_drawDeclinedWindowIsOpen    = openOrClose;}
    void openOrClosePairDeclineWindow     (bool openOrClose) {m_pairingDeclinedWindowIsOpen = openOrClose;}

    void renderAllTheThings();

    auto getSquareSizeInPixels()    const {return m_squareSize;}
    auto getMenuBarHeightInPixels() const {return m_menuBarHeight;}
    auto getBoardWidthInPixels()    const {return m_chessBoardWidth;}
    auto getBoardHeightInPixels()   const {return m_chessBoardHeight;}
    bool isPromotionWindowOpen()    const {return m_promotionWindowIsOpen;}

    //indecies into the array of SDL textures for the different pieces.
    //scoped to ChessDrawer but is an unscoped enum (not enum struct/class)  
    //to allow for convenient implicit conversion to the underlying type 
    //(the textures are in the single chess app instance as m_pieceTextures)
    enum TextureIndices : uint32_t
    {
        WPAWN, WKNIGHT, WROOK, WBISHOP, WQUEEN, WKING,
        BPAWN, BKNIGHT, BROOK, BBISHOP, BQUEEN, BKING, INVALID
    };

private:

    void drawDrawOfferPopup();
    void drawIDNotInLobbyPopup();
    void drawPiecesNotOnMouse();
    void drawPieceOnMouse();
    void drawMoveIndicatorCircles();
    void drawDrawDeclinedPopup();
    void drawColorEditorWindow();
    void drawConnectionWindow();
    void drawPromotionPopup();
    void drawMenuBar();
    void drawResetButtonErrorPopup();
    void drawPairingCompletePopup();
    void drawWinLossDrawPopup();
    void drawRematchRequestPopup();
    void drawPairRequestPopup();
    void drawPairDeclinePopup();
    void drawSquares();

    void loadPieceTexturesFromDisk(std::array<std::string, NUMOF_PIECE_TEXTURES> const& filePaths);
    
    //generates a circle texture at startup to use later
    void initCircleTexture(int radius, Uint8 RR, Uint8 GG, Uint8 BB, Uint8 AA, 
        SDL_Texture** toInit);

    void serializeSquareColorData();
    void deserializeAndLoadSquareColorData();

    static void pushMenuBarStyles();
    static void popMenuBarStyles();

    static void centerNextWindow();

private:

    bool m_colorEditorWindowIsOpen     {false};
    bool m_demoWindowIsOpen            {false};
    bool m_promotionWindowIsOpen       {false};
    bool m_connectionWindowIsOpen      {false};
    bool m_resetBoardWindowIsOpen      {false};
    bool m_pairingCompleteWindowIsOpen {false};
    bool m_winLossDrawWindowIsOpen     {false};
    bool m_rematchRequestWindowIsOpen  {false};
    bool m_pairRequestWindowIsOpen     {false};
    bool m_IDNotInLobbyWindowIsOpen    {false};
    bool m_drawOfferWindowIsOpen       {false};
    bool m_drawDeclinedWindowIsOpen    {false};
    bool m_pairingDeclinedWindowIsOpen {false};

    uint32_t m_squareSize;
    uint32_t m_chessBoardWidth;
    uint32_t m_chessBoardHeight;
    float m_menuBarHeight;

    std::array<SDL_Texture*, NUMOF_PIECE_TEXTURES> m_pieceTextures;
    std::array<Vec2i,        NUMOF_PIECE_TEXTURES> m_pieceTextureSizes;
    std::array<uint32_t, 4> m_lightSquareColor, m_darkSquareColor;

    SDL_Texture* m_circleTexture;
    SDL_Texture* m_redCircleTexture;
    std::string m_winLossDrawPopupMessage;//message that gets displayed when the game is over
};