#pragma once
#include <array>
#include <cstdint>
#include "SDL.h"
#include "Vector2i.hpp"
#include "PopupManager.hpp"
#include "ChessEvents.hpp"
#include "TextureManager.hpp"
#include "Window.hpp"
#include "chessNetworkProtocol.h" //enum Side

class SettingsManager;
class Board;
class ConnectionManager;

class ChessRenderer
{
public:

    ChessRenderer(NetworkEventSystem::Subscriber&,
        BoardEventSystem::Subscriber&, GUIEventSystem::Publisher const&);

    ~ChessRenderer();

    auto static getSquareSize() {return sSquareSize;}

    bool isScreenPositionOnBoard(Vec2i) const;
    void renderAllTheThings(Board const& b, ConnectionManager const& cm);
    Vec2i screen2ChessPos(Vec2i) const;

    struct PromotionWindowContext
    {
        //promotingSide is the side (white or black) that is promoting, 
        //not the side of the board the promotion is happening on.
        Side  promotingSide;
        Vec2i promotingSquare;
    };

private:

    //Methods to reduce the size of subToEvents()
    void onPairingCompleteEvent(NetworkEvents::PairingComplete const&);
    void onDrawDeclinedEvent();
    void onIDNotInLobbyEvent(NetworkEvents::IDNotInLobby const&);
    void onDrawOfferEvent();
    void onRematchRequestEvent();
    void onRematchDeclineEvent();
    void onPairRequestEvent(NetworkEvents::PairRequest const&);
    void onPairDeclineEvent();
    void onGameOverEventWhilePaired(BoardEvents::GameOver const & evnt);
    void onGameOverEventWhileNotPaired(BoardEvents::GameOver const&);
    void onUnpairEvent();
    void onPromotionBeginEvent(BoardEvents::PromotionBegin const&);
    void onDisconnectedEvent();
    void onConnectedEvent();
    void onPairRequestWhilePairedEvent();
    void onRematchAcceptEvent();
    void onOpponentHasResignedEvent();
    void onDrawAcceptedEvent();

    void drawPromotionWindow();
    void drawColorEditor();
    void drawConnectionWindow();
    void drawMoveIndicatorCircles(Board const&);
    void drawToTheBoardTexture(Board const&);
    void drawMainWindow();
    void drawPieceOnMouse();
    void drawSquares();
    void drawPiecesNotOnMouse(Board const&);
    void drawMenuBar(Board const&, ConnectionManager const&);

    //methods to reduce ctor/dtor size
    std::string getLightSquareColorAsString();
    std::string getDarkSquareColorAsString();
    void generateNewSquareColorDataTextFile(SettingsManager const& settingsManager);
    void initSquareColorData();
    void serializeSquareColorData();
    void subToEvents();
    
    enum struct NetworkSubscriptions
    {
        DRAW_ACCEPTED,
        OPPONENT_RESIGNED,
        PAIR_REQUEST_WHILE_PAIRED,
        PAIRING_COMPLETE,
        DRAW_DECLINED,
        ID_NOT_IN_LOBBY,
        DRAW_OFFER,
        REMATCH_REQUEST,
        REMATCH_DECLINE,
        REMATCH_ACCEPT,
        PAIR_REQUEST,
        PAIR_DECLINE,
        UNPAIR,
        DISCONNECTED,
        CONNECTED,
        NEW_ID
    };

    void addRematchAndUnpairPopupButtons();

    SubscriptionManager<NetworkSubscriptions,
        NetworkEventSystem::Subscriber> mNetworkSubManager;

    BoardEventSystem::Subscriber& mBoardSubscriber;
    SubscriptionID mGameOverSubID {INVALID_SUBSCRIPTION_ID};
    SubscriptionID mPromotionBeginEventID {INVALID_SUBSCRIPTION_ID};

    //Takes a chess position, and returns the pixel screen coords
    //of where that is (the middle of the square).
    Vec2i chess2ScreenPos(Vec2i);

private:

    inline static uint32_t sSquareSize   {112U};
    inline static uint32_t sWindowWidth  {sSquareSize * 8};
    inline static uint32_t sWindowHeight {sWindowWidth};

    ImVec2 mMenuBarSize {};
    Window mWindow
    {
        static_cast<int>(sWindowWidth), static_cast<int>(sWindowHeight), 
        "Chess", SDL_INIT_VIDEO | SDL_INIT_AUDIO, 0
    };

    Side mViewingPerspective {Side::WHITE};

    TextureManager mTextureManager {mWindow.renderer};
    PopupManager mPopupManager;

    GUIEventSystem::Publisher const& mGuiEventPublisher;

    bool mIsColorEditorWindowOpen {false};
    bool mIsConnectionWindowOpen  {false};
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