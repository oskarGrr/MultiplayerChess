#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include <functional>//std::invoke //std::function
#include <span>
#include <type_traits>
#include <ConnectionManager.hpp>
#include <optional>

#include "ChessEvents.hpp"
#include "Board.hpp"
#include "ChessRenderer.hpp"
#include "PieceTypes.h"
#include "Vector2i.h"
#include "errorLogger.h"
#include "SettingsFileManager.hpp"
#include "chessNetworkProtocol.h" //enum Side

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "SDL.h"
#include "SDL_image.h"

//board width, height and square size are initialized with their setters later
ChessRenderer::ChessRenderer(NetworkEventSystem::Subscriber& networkEventSys, 
    BoardEventSystem::Subscriber& boardEventSubscriber, GUIEventSystem::Publisher const& guiEventPublisher) 
    : mGuiEventPublisher{guiEventPublisher}, 
      mNetworkSubManager{networkEventSys}, 
      mBoardSubscriber{boardEventSubscriber}
{
    subToEvents();
    ImGui::StyleColorsLight();
    initSquareColorData();
}

ChessRenderer::~ChessRenderer()
{
    mBoardSubscriber.unsub<BoardEvents::GameOver>(mGameOverSubID);
    mBoardSubscriber.unsub<BoardEvents::PromotionBegin>(mPromotionBeginEventID);

    //mNetworkSubManager will automatically unsub from the rest of the events...
}

void ChessRenderer::initSquareColorData()
{
    auto const getDataIfCorrect = [](std::string const& colorString) 
        -> std::optional< std::array<uint32_t, 4> >
    {
        std::istringstream iss{colorString};
        std::array<uint32_t, 4> ret{};
        int i = 0;

        while(iss && i < ret.size())
        {
            std::string substr;
            iss >> substr;

            //if substr is empty or has any non number characters
            if(substr.empty() || std::find_if(substr.begin(), substr.end(),
                [](auto c) { return ! std::isdigit(c); } ) != substr.end())
            {
                return std::nullopt;
            }
            
            uint32_t x { std::strtoul(substr.data(), nullptr, 10) };

            if(x > 255) { return std::nullopt; }

            ret[i] = x;
            ++i;
        }

        return ret;
    };

    //initialize the light/dark square colors with the data found in squareColorData.txt
    //otherwise the light and dark squares will be the default color.
    SettingsManager squareColorDataManager{"squareColorData.txt"};
    auto maybeLightSquareStr {squareColorDataManager.getValue("L")};
    auto maybeDarkSquareStr  {squareColorDataManager.getValue("D")};
    
    if( ! maybeLightSquareStr || ! maybeDarkSquareStr )
        return;

    auto maybeLightSquareColor { getDataIfCorrect(*maybeLightSquareStr) };
    auto maybeDarkSquareColor  { getDataIfCorrect(*maybeDarkSquareStr) };

    if( ! maybeLightSquareColor || ! maybeDarkSquareColor )
        return;

    mLightSquareColor = *maybeLightSquareColor;
    mDarkSquareColor  = *maybeDarkSquareColor;
}

void ChessRenderer::drawSquares()
{
    SDL_Rect square{0, static_cast<int>(mMenuBarSize.y), 
        static_cast<int>(sSquareSize), static_cast<int>(sSquareSize)};

    for(int i = 0; i < 8; ++i, square.x += sSquareSize)
    {
        for(int j = 0; j < 8; ++j, square.y += sSquareSize)
        {
            if( ! (i + j & 1) )//if light square
            {
                //scale up the colors from 0-1 to 0-255 and draw
                SDL_SetRenderDrawColor
                (
                    mWindow.renderer,
                    mLightSquareColor[0],
                    mLightSquareColor[1],
                    mLightSquareColor[2],
                    mLightSquareColor[3]
                );
            }
            else//if dark square
            {
                SDL_SetRenderDrawColor
                (
                    mWindow.renderer,
                    mDarkSquareColor[0],
                    mDarkSquareColor[1],
                    mDarkSquareColor[2],
                    mDarkSquareColor[3]
                );
            }

            SDL_RenderFillRect(mWindow.renderer, &square);
        }

        square.y = static_cast<int>(mMenuBarSize.y);
    }
}

struct MenuBarStyles //RAII style push and pop imgui styles
{
    MenuBarStyles() 
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {9,5});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {.011f, 0.615f, 0.988f, .75f});
        ImGui::PushStyleColor(ImGuiCol_Separator, {0,0,0,1});
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, {183/255.f, 189/255.f, 188/255.f, 1});
        ImGui::PushStyleColor(ImGuiCol_Button, {.2f, 0.79f, 0.8f, 0.70f});
    }

    ~MenuBarStyles()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
};

void ChessRenderer::drawMenuBar(Board const& b, ConnectionManager const& cm)
{
    [[maybe_unused]] MenuBarStyles styles; //RAII style push and pop imgui styles
    
    if(ImGui::BeginMainMenuBar()) [[unlikely]]
    {
        if(ImGui::BeginMenu("options"))
        {
            if(ImGui::MenuItem("change square colors", nullptr, nullptr))
                mIsColorEditorWindowOpen = true;

            if(ImGui::MenuItem("connect to another player", nullptr, nullptr))
                mIsConnectionWindowOpen = true;

            ImGui::EndMenu();
        }

        if(ImGui::SmallButton("flip board"))
        {
            mViewingPerspective = mViewingPerspective == Side::WHITE ? Side::BLACK : Side::WHITE;
        }

        if(ImGui::SmallButton("reset board"))
        {
            mIsPromotionWindowOpen = false;

            if(cm.isPairedOnline())
            {
                mPopupManager.startNewPopup(
                    "You can't reset the board while connected with another player.", true
                );
            }
            else 
            {
                GUIEvents::ResetBoard evnt{};
                mGuiEventPublisher.pub(evnt);
            }
        }

        if(cm.isConnectedToServer())
        {
            if(cm.isPairedOnline())
            {
                if(ImGui::SmallButton("resign"))
                {
                    GUIEvents::Resign resignEvnt{};
                    mGuiEventPublisher.pub(resignEvnt);

                    mPopupManager.startNewPopup("You lost (you resigned).", true);
                }

                if(ImGui::SmallButton("draw"))
                {
                    GUIEvents::DrawOffer evnt{};
                    mGuiEventPublisher.pub(evnt);
                }
            }

            ImGui::Separator();
            ImGui::Text("connected to server");
            ImGui::Separator();
            ImGui::Text("your ID: %u", cm.getUniqueID());
            ImGui::Separator();

            if(cm.isPairedOnline())
                ImGui::Text("opponentID: %u", cm.getOpponentID());
        }
        else
        {
            ImGui::Separator();
            ImGui::TextUnformatted("not connected to server");
            ImGui::Separator();
        }

        auto whosTurn {b.getWhosTurnItIs() == Side::WHITE ? "it's white's turn" : "it's black's turn"};
        ImGui::SameLine( -(ImGui::CalcTextSize(whosTurn).x + 18) );
        ImGui::TextUnformatted(whosTurn);

        static bool firstPass {true};
        if(firstPass) [[unlikely]]
        {
            mMenuBarSize = ImGui::GetWindowSize();
            SDL_SetWindowSize(mWindow.window, static_cast<int>(sWindowWidth), 
                static_cast<int>(sWindowHeight + mMenuBarSize.y));
        }

        ImGui::EndMainMenuBar();
    }
}

void ChessRenderer::drawPiecesNotOnMouse(Board const& b)
{
    auto const pom { Piece::getPieceOnMouse() };

    for(auto const& piece : b.getPieces())
    {
        if(piece && piece != pom)
        {
            //figure out where on the screen the piece is (the middle of the square)
            Vec2i screenPosition { chess2ScreenPos(piece->getChessPosition()) };

            auto const& texture { mTextureManager.getTexture(piece->getWhichTexture()) };

            //get the width and height of whichever texture the piece on the mouse is
            Vec2i textureSize { texture.getSize() };
            
            //from the screen position figure out the destination rectangle
            SDL_Rect destination
            {
                .x = screenPosition.x - textureSize.x / 2,
                .y = screenPosition.y - textureSize.y / 2,
                .w = textureSize.x, 
                .h = textureSize.y
            };
            
            SDL_RenderCopy(mWindow.renderer,texture.getTexture().get(), nullptr, &destination);
        }
    }
}

void ChessRenderer::drawPieceOnMouse()
{
    auto const pom { Piece::getPieceOnMouse() };

    if(pom)
    {
        Vec2i mousePosition{};
        SDL_GetMouseState(&mousePosition.x, &mousePosition.y);

        auto const& pieceTex { mTextureManager.getTexture(pom->getWhichTexture()) };
        auto const pieceTexSize { pieceTex.getSize() };

        SDL_Rect destination
        {
            .x = mousePosition.x - pieceTexSize.x / 2,
            .y = mousePosition.y - pieceTexSize.y / 2,
            .w = pieceTexSize.x, 
            .h = pieceTexSize.y
        };

        SDL_RenderCopy(mWindow.renderer, pieceTex.getTexture().get(), nullptr, &destination);
    }
}

void ChessRenderer::drawMoveIndicatorCircles(Board const& b)
{
    auto const pom = Piece::getPieceOnMouse();
    if( ! pom ) return;

    auto const& redCircleTex   {mTextureManager.getTexture(TextureManager::WhichTexture::RED_CIRCLE)};
    auto const& grayCircleTex  {mTextureManager.getTexture(TextureManager::WhichTexture::GRAY_CIRCLE)};
    auto const  redCircleSize  {redCircleTex.getSize()};
    auto const  grayCircleSize {grayCircleTex.getSize()};

    for(auto const& move : pom->getLegalMoves())
    {
        Vec2i const circlePos{chess2ScreenPos(move.dest)};

        SDL_Rect const redCircleDest
        {
            static_cast<int>(circlePos.x - redCircleSize.x / 2),
            static_cast<int>(circlePos.y - redCircleSize.x / 2),
            static_cast<int>(redCircleSize.x),
            static_cast<int>(redCircleSize.y)
        };
        SDL_Rect const grayCircleDest
        {
            static_cast<int>(circlePos.x - grayCircleSize.x / 2),
            static_cast<int>(circlePos.y - grayCircleSize.x / 2),
            static_cast<int>(grayCircleSize.x),
            static_cast<int>(grayCircleSize.y)
        };

        //If this legal move is a capture of another piece draw a red circle, otherwise draw a gray circle.
        if(move.wasCapture)
            SDL_RenderCopy(mWindow.renderer, redCircleTex.getTexture().get(), nullptr, &redCircleDest);
        else 
            SDL_RenderCopy(mWindow.renderer, grayCircleTex.getTexture().get(), nullptr, &grayCircleDest);
    }
}

void ChessRenderer::renderAllTheThings(Board const& b, ConnectionManager const& cm)
{
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    mPopupManager.drawPopup();

    //ImGui::ShowDemoWindow();

    if(mIsColorEditorWindowOpen) [[unlikely]]
        drawColorEditor();

    if(mIsConnectionWindowOpen)  [[unlikely]]
        drawConnectionWindow();

    if(mIsPromotionWindowOpen)   [[unlikely]]
        drawPromotionPopup();

    drawMenuBar(b, cm);

    ImGui::Render();
    SDL_RenderClear(mWindow.renderer);

    drawSquares();
    drawPiecesNotOnMouse(b);
    if( ! mIsPromotionWindowOpen ) [[likely]]
    {
        drawMoveIndicatorCircles(b);
        drawPieceOnMouse();
    }

    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(mWindow.renderer);
}

struct PromotionImguiStyles //RAII style imgui styles for the promotion popup
{
    PromotionImguiStyles()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {1.0f, 1.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   {0.0f, 0.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  {0.0f, 1.0f});
    }
    ~PromotionImguiStyles() { ImGui::PopStyleVar(4); }
};

void ChessRenderer::drawPromotionPopup()
{
    PromotionImguiStyles raiiStyles;
    
    ImGui::Begin("pick a piece!", nullptr, ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBackground);

    Vec2i promoScreenPos {chess2ScreenPos(mPromotionContext.promotingSquare)};
    promoScreenPos.x -= sSquareSize / 2;
    promoScreenPos.y -= sSquareSize / 2;

    Side const promoSide {mPromotionContext.promotingSide}; //shorter name

    if(promoSide != mViewingPerspective)
        promoScreenPos.y -= sSquareSize * 3;
        
    using enum TextureManager::WhichTexture;
    auto const& queenTex  {mTextureManager.getTexture(promoSide == Side::WHITE ? WHITE_QUEEN  : BLACK_QUEEN)};
    auto const& rookTex   {mTextureManager.getTexture(promoSide == Side::WHITE ? WHITE_ROOK   : BLACK_ROOK)};
    auto const& knightTex {mTextureManager.getTexture(promoSide == Side::WHITE ? WHITE_KNIGHT : BLACK_KNIGHT)};
    auto const& bishopTex {mTextureManager.getTexture(promoSide == Side::WHITE ? WHITE_BISHOP : BLACK_BISHOP)};

    ImGui::SetWindowPos(promoScreenPos);

    if(ImGui::ImageButton(static_cast<ImTextureID>(queenTex.getTexture().get()), queenTex.getSize(), 
        {}, {1, 1}, -1, {}, {1, 1, 1, 0.25f}))
    {
        GUIEvents::PromotionEnd evnt{ChessMove::PromoTypes::QUEEN};
        mGuiEventPublisher.pub(evnt);
        mIsPromotionWindowOpen = false;
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(rookTex.getTexture().get()), rookTex.getSize(), 
        {}, {1, 1}, -1, {}, {1, 1, 1, 0.25f}))
    {
        GUIEvents::PromotionEnd evnt{ChessMove::PromoTypes::ROOK};
        mGuiEventPublisher.pub(evnt);
        mIsPromotionWindowOpen = false;
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(knightTex.getTexture().get()), knightTex.getSize(), 
        {}, {1, 1}, -1, {}, {1, 1, 1, 0.25f}))
    {
        GUIEvents::PromotionEnd evnt{ChessMove::PromoTypes::KNIGHT};
        mGuiEventPublisher.pub(evnt);
        mIsPromotionWindowOpen = false;
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(bishopTex.getTexture().get()),  bishopTex.getSize(),
        {}, {1, 1}, -1, {}, {1, 1, 1, 0.25f}))
    {
        GUIEvents::PromotionEnd evnt{ChessMove::PromoTypes::BISHOP};
        mGuiEventPublisher.pub(evnt);
        mIsPromotionWindowOpen = false;
    }

    ImGui::End();
}

void ChessRenderer::drawColorEditor()
{
    ImGui::Begin("change square colors", &mIsColorEditorWindowOpen,
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::TextUnformatted("light squares");
    
    ImVec4 f_lightSquares{};//a float (0-1) version of the light squares
    ImVec4 f_darkSquares{};//a float (0-1) version of the dark squares
    for(int i = 0; i < 4; ++i)
    {
        f_lightSquares[i] = mLightSquareColor[i] / 255.0f;
        f_darkSquares[i]  = mDarkSquareColor[i]  / 255.0f;
    }
    
    ImGui::ColorPicker3("light squares", &f_lightSquares[0]);

    if(ImGui::SmallButton("default light squares"))
        for(int i = 0; i < 4; ++i)
            f_lightSquares[i] = mDefaultLightSquareColor[i] / 255.0f;

    ImGui::Separator();

    ImGui::TextUnformatted("dark squares");
    ImGui::ColorPicker3("dark squares", &f_darkSquares[0]);
    
    if(ImGui::SmallButton("default dark squares"))
        for(int i = 0; i < 4; ++i)
            f_darkSquares[i] = mDefaultDarkSquareColor[i] / 255.0f;

    for(int i = 0; i < 4; ++i)
    {
        mLightSquareColor[i] = static_cast<Uint8>(f_lightSquares[i] * 255);
        mDarkSquareColor[i] = static_cast<Uint8>(f_darkSquares[i] * 255);
    }

    ImGui::End();
}

bool ChessRenderer::isScreenPositionOnBoard(Vec2i const screenPos) const
{
    //ImGui::IsWindowHovered() should handle this. There seems to be a bug in their code.
    //It does not work in the case where I click on the options menu bar 
    //drop down, and then click again on it while the drop down is open.
    bool const belowMenuBar{screenPos.y > static_cast<int>(mMenuBarSize.y)};

    return ! ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && belowMenuBar;
}

static bool isIDStringValid(std::string_view opponentID)
{
    //The ID is a uint32_t as a string in base 10. The string referenced by
    //opponentID should not exceed 10 chars. '\0' is not counted by size().
    if(opponentID.size() > 10 || opponentID.size() == 0)
        return false;

    //if any of the chars are not 0-9
    for(auto const& c : opponentID)
    {
        if( ! std::isdigit(c) )
            return false;
    }

    uint64_t bigID = std::strtoull(opponentID.data(), nullptr, 10);

    if(bigID > UINT32_MAX) 
        return false;

    return true;
}

void ChessRenderer::drawConnectionWindow()
{
    ImGui::Begin("connect to another player", &mIsConnectionWindowOpen,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    
    static bool isInputValid{true};

    char opponentID[11] = {0};
    
    ImGui::TextUnformatted("Enter the ID of the player you wish to play against.");
    ImGui::TextUnformatted("If you are connected to the server then your ID will");
    ImGui::TextUnformatted("be at the top of the window in the title bar.");

    if(ImGui::InputTextWithHint("##opponentID", "opponent's ID", opponentID, sizeof(opponentID), 
        ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if(isIDStringValid(opponentID))
        {
            GUIEvents::PairRequest evnt{std::strtoul(opponentID, nullptr, 10)};
            mGuiEventPublisher.pub(evnt);
            isInputValid = true;
        }
        else isInputValid = false;
    }

    if( ! isInputValid )
        ImGui::TextUnformatted("Invalid ID");

    ImGui::End();
}

void ChessRenderer::onPairingCompleteEvent(NetworkEvents::PairingComplete const& evnt)
{
    std::string popupText {"you are playing with the "};
    popupText.append(evnt.side == Side::WHITE ? "white " : "black ");
    popupText.append(std::format("pieces against the user with ID: {}", evnt.opponentID));
    
    mPopupManager.startNewPopup(std::move(popupText), false);//careful popupText has been moved from!
    mPopupManager.addButton( {"Let's play!", []{return true;} } );
    
    mViewingPerspective = evnt.side;
    mIsConnectionWindowOpen = false;

    mBoardSubscriber.unsub<BoardEvents::GameOver>(mGameOverSubID);

    mGameOverSubID = mBoardSubscriber.sub<BoardEvents::GameOver>(
        [this](Event const& e){onGameOverEventWhilePaired(e.unpack<BoardEvents::GameOver>());
    });
}

void ChessRenderer::onDrawDeclinedEvent()
{
    mPopupManager.startNewPopup("draw declined", true);
}

void ChessRenderer::onIDNotInLobbyEvent(NetworkEvents::IDNotInLobby const& evnt)
{
    mPopupManager.startNewPopup(
        std::format("The ID you supplied ({}) was invalid", evnt.ID), true
    );
}

void ChessRenderer::onDrawOfferEvent()
{
    mPopupManager.startNewPopup("Your opponent has offered a draw.", false);

    mPopupManager.addButton({
        .text = "Accept",
        .callback = [this]{
            GUIEvents::DrawAccept evnt{};
            mGuiEventPublisher.pub(evnt);
            return true;
        }
    });

    mPopupManager.addButton({
        .text = "Decline",
        .callback = [this]{
            GUIEvents::DrawDecline evnt{};
            mGuiEventPublisher.pub(evnt);
            return true;
        }
    });
}

void ChessRenderer::onRematchRequestEvent()
{
    mPopupManager.startNewPopup("Your opponent offered a rematch", false);

    mPopupManager.addButton({
        .text = "accept rematch",
        .callback = [this]
        {
            GUIEvents::RematchAccept evnt{};
            mGuiEventPublisher.pub(evnt); return true;       
        }     
    });

    mPopupManager.addButton({
        .text = "decline rematch",
        .callback = [this]
        {
            mPopupManager.startNewPopup(
                "You have been disconnected from your opponent and put back in the server lobby", true
            );
        
            GUIEvents::RematchDecline evnt{};
            mGuiEventPublisher.pub(evnt);
            return true;
        }
    });
}

void ChessRenderer::onRematchDeclineEvent()
{
    mPopupManager.startNewPopup(
        "You have been disconnected from your opponent and put back in the server lobby", true
    );
}

void ChessRenderer::onPairRequestEvent(NetworkEvents::PairRequest const& evnt)
{
    mPopupManager.startNewPopup(std::format(
        "Request from {} to play chess!", evnt.potentialOpponentID), false
    );

    mPopupManager.addButton({
        "Accept", 
        [this]{
            GUIEvents::PairAccept evnt{};
            mGuiEventPublisher.pub(evnt); return true;
        }
    });
    
    mPopupManager.addButton({
        "Decline", 
        [this]{
            GUIEvents::PairDecline evnt{};
            mGuiEventPublisher.pub(evnt); return true;
        }
    });
}

void ChessRenderer::onPairDeclineEvent()
{
    mPopupManager.startNewPopup("Your offer to play chess was declined", true);
}

void ChessRenderer::onGameOverEventWhilePaired(BoardEvents::GameOver const& evnt)
{
    mPopupManager.startNewPopup(evnt.reason, false);

    mPopupManager.addButton({
        "Request rematch", 
        [this]{ GUIEvents::RematchRequest e; mGuiEventPublisher.pub(e);  return true;}
    });

    mPopupManager.addButton({
        "Disconnect from opponent",
        [this]
        {
            GUIEvents::Unpair e; mGuiEventPublisher.pub(e);
            mViewingPerspective = Side::WHITE;
            return true;
        }
    });
}

void ChessRenderer::onGameOverEventWhileNotPaired(BoardEvents::GameOver const& evnt)
{
    mPopupManager.startNewPopup(evnt.reason, true);
}

void ChessRenderer::onUnpairEvent()
{
    mPopupManager.startNewPopup("You have been unpaired with your opponent", true);

    mViewingPerspective = Side::WHITE;

    mBoardSubscriber.unsub<BoardEvents::GameOver>(mGameOverSubID);

    mGameOverSubID = mBoardSubscriber.sub<BoardEvents::GameOver>(
        [this](Event const& e){ onGameOverEventWhileNotPaired(e.unpack<BoardEvents::GameOver>()); 
    });
}

void ChessRenderer::onPromotionBeginEvent(BoardEvents::PromotionBegin const& evnt)
{
    mIsPromotionWindowOpen = true;
    mPromotionContext.promotingSide = evnt.promotingSide;
    mPromotionContext.promotingSquare = evnt.promotingSquare;
}

void ChessRenderer::onDisconnectedEvent()
{
    mViewingPerspective = Side::WHITE;
    mPopupManager.startNewPopup("You are no longer connected to the server.", true);
}

void ChessRenderer::onConnectedEvent()
{
    mPopupManager.startNewPopup("You have successfully connected to the server.", true);
}

void ChessRenderer::subToEvents()
{
    mNetworkSubManager.sub<NetworkEvents::PairingComplete>(NetworkSubscriptions::PAIRING_COMPLETE,
        [this](Event const& e){ onPairingCompleteEvent( e.unpack<NetworkEvents::PairingComplete>() ); });

    mNetworkSubManager.sub<NetworkEvents::DrawDeclined>(NetworkSubscriptions::DRAW_DECLINED, 
        [this](Event const&){ onDrawDeclinedEvent(); });

    mNetworkSubManager.sub<NetworkEvents::IDNotInLobby>(NetworkSubscriptions::ID_NOT_IN_LOBBY,
        [this](Event const& e){ onIDNotInLobbyEvent( e.unpack<NetworkEvents::IDNotInLobby>() ); });

    mNetworkSubManager.sub<NetworkEvents::DrawOffer>(NetworkSubscriptions::DRAW_OFFER,
        [this](Event const&){ onDrawOfferEvent(); });
    
    mNetworkSubManager.sub<NetworkEvents::RematchRequest>(NetworkSubscriptions::REMATCH_REQUEST,
        [this](Event const&){ onRematchRequestEvent(); });

    mNetworkSubManager.sub<NetworkEvents::RematchDecline>(NetworkSubscriptions::REMATCH_DECLINE,
        [this](Event const&){ onRematchDeclineEvent(); });

    mNetworkSubManager.sub<NetworkEvents::PairRequest>(NetworkSubscriptions::PAIR_REQUEST,
        [this](Event const& e){ onPairRequestEvent( e.unpack<NetworkEvents::PairRequest>() ); });

    mNetworkSubManager.sub<NetworkEvents::PairDecline>(NetworkSubscriptions::PAIR_DECLINE, 
        [this](Event const&){ onPairDeclineEvent(); });

    mNetworkSubManager.sub<NetworkEvents::Unpair>(NetworkSubscriptions::UNPAIR,
        [this](Event const&){ onUnpairEvent(); });

    mNetworkSubManager.sub<NetworkEvents::DisconnectedFromServer>(NetworkSubscriptions::DISCONNECTED,
        [this](Event const&){ onDisconnectedEvent(); });

    mNetworkSubManager.sub<NetworkEvents::ConnectedToServer>(NetworkSubscriptions::CONNECTED,
        [this](Event const&){ onConnectedEvent(); });

    mGameOverSubID = mBoardSubscriber.sub<BoardEvents::GameOver>([this](Event const& e){ 
         onGameOverEventWhileNotPaired(e.unpack<BoardEvents::GameOver>());
    });

    mPromotionBeginEventID = mBoardSubscriber.sub<BoardEvents::PromotionBegin>([this](Event const& e){
        onPromotionBeginEvent(e.unpack<BoardEvents::PromotionBegin>() );
    });
}

//Takes a chess position, and returns the pixel screen coords 
//of where that is (the middle of the square).
Vec2i ChessRenderer::chess2ScreenPos(Vec2i const pos)
{
    Vec2i ret{pos};

    if(mViewingPerspective == Side::WHITE)
        ret.y = 7 - ret.y;
    else//if viewing the board from blacks perspective.
        ret.x = 7 - ret.x;

    //scale up to board size
    ret.x *= sSquareSize;
    ret.y *= sSquareSize;

    //move from top left to middle of square
    ret.y += static_cast<int>(sSquareSize / 2);
    ret.x += static_cast<int>(sSquareSize / 2);

    //move down to account for the menu bar.
    //will only work after the first frame since the first imgui frame
    //needs to be started to measure the menu bar
    ret.y += static_cast<int>(mMenuBarSize.y);

    return ret;
}

//Will not check if pos is on the chess board.
Vec2i ChessRenderer::screen2ChessPos(Vec2i const pos) const
{
    auto const menuBarHeight = static_cast<int>(mMenuBarSize.y);

    Vec2i ret
    {
        static_cast<int>(pos.x / sSquareSize),
        static_cast<int>((pos.y - menuBarHeight) / sSquareSize)
    };

    if(mViewingPerspective == Side::WHITE) { ret.y = 7 - ret.y; }
    else { ret.x = 7 - ret.x; }

    return ret;
}