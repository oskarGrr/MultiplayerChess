#pragma once
#include <array>
#include <cstdint>
#include <unordered_map>
#include <string>
#include "SDL.h"
#include "Vector2i.h"

//this class is for drawing the necessary stuff
//with SDL and imgui for the chess application like
//the squares pieces and imgui windows etc

#define NUMOF_PIECE_TEXTURES 12

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

    bool isScreenPositionOnBoard(Vec2i const screenPos) const;

    //These are used as a key to map to the draw __ popup/window methods below.
    //I would use a std::unordered_set, and just have member function pointers as the key,
    //but you can't easily hash those, so these are the keys in an unordered map. 
    //The values they map to are of course of type void(ChessDrawer::*)() which 
    //will point to the correct draw __ window/popup method.
    enum struct WindowTypes : uint32_t
    {
        UNPAIR,
        DRAW_OFFER,
        ID_NOT_IN_LOBBY,
        DRAW_DECLINED,
        COLOR_EDITOR,
        CONNECTION,
        PROMOTION,
        RESET_BOARD_ERROR,
        PAIRING_COMPLETE,
        WIN_LOSS_DRAW,
        REMATCH_REQUEST,
        PAIR_REQUEST,
        PAIRING_DECLINED
    };

    void openWindow(WindowTypes);
    void closeWindow(WindowTypes);

    void renderAllTheThings();

    auto getSquareSizeInPixels()    const {return m_squareSize;}
    auto getMenuBarSize()           const {return m_menuBarSize;}
    auto getBoardWidthInPixels()    const {return m_chessBoardWidth;}
    auto getBoardHeightInPixels()   const {return m_chessBoardHeight;}
    bool isPromotionWindowOpen()    const;

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

    void deferedWindowDeletion();

    //Draw window/popup methods
    void drawDrawOfferPopup();
    void drawIDNotInLobbyPopup();
    void drawDrawDeclinedPopup();
    void drawColorEditorWindow();
    void drawConnectionWindow();
    void drawPromotionPopup();
    void drawResetBoardErrorPopup();
    void drawPairingCompletePopup();
    void drawWinLossDrawPopup();
    void drawRematchRequestPopup();
    void drawPairRequestPopup();
    void drawPairingDeclinedPopup();
    void drawUnpairPopup();
    void drawMoveIndicatorCircles();
    void drawPieceOnMouse();
    void drawMenuBar();
    void drawSquares();
    void drawPiecesNotOnMouse();

    void loadPieceTexturesFromDisk(std::array<std::string, NUMOF_PIECE_TEXTURES> const& filePaths);
    
    //generates a circle texture at startup to use later
    void initCircleTexture(int radius, Uint8 RR, Uint8 GG, Uint8 BB, Uint8 AA, 
        SDL_Texture** toInit);

    void serializeSquareColorData();
    void deserializeAndLoadSquareColorData();

    static void pushMenuBarStyles();
    static void popMenuBarStyles();

    static void centerNextWindow();

    //The bool in the std::pair is to tell whether or not that window is open.
    //If you try to open a window that is already open, or close a window that is already closed
    //then nothing happens. Otherwise we will push_back() to the m_openWindows vector.
    std::unordered_map<WindowTypes, std::pair<void(ChessDrawer::*)(void), bool>> m_windowFunctionMap;

    //The bool is the std::pair is to mark a vector element for defered deletion.
    std::vector<std::pair<void(ChessDrawer::*)(void), bool>> m_openWindows;

    uint32_t m_squareSize;
    uint32_t m_chessBoardWidth;
    uint32_t m_chessBoardHeight;
    ImVec2   m_menuBarSize;
    bool m_imguiDemoWindowIsOpen{false};

    std::array<SDL_Texture*, NUMOF_PIECE_TEXTURES> m_pieceTextures;
    std::array<Vec2i,        NUMOF_PIECE_TEXTURES> m_pieceTextureSizes;
    std::array<uint32_t, 4> m_lightSquareColor, m_darkSquareColor;

    SDL_Texture* m_circleTexture;
    SDL_Texture* m_redCircleTexture;

    //The message that gets displayed in a popup when the game is over.
    std::string m_winLossDrawPopupMessage;
};