#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include <functional>//std::invoke //std::function
#include <span>
#include <type_traits>
#include <ConnectionManager.hpp>
#include <optional>
#include <cmath>//atan2, cos, sin

#include "ChessEvents.hpp"
#include "Board.hpp"
#include "ChessRenderer.hpp"
#include "PieceTypes.hpp"
#include "Vector2i.hpp"
#include "errorLogger.hpp"
#include "SettingsFileManager.hpp"
#include "chessNetworkProtocol.h" //enum Side

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "SDL.h"
#include "SDL_image.h"

static auto const squareColorDataFname {"squareColorData.txt"};

static SDL_HitTestResult hitTestCallback(SDL_Window *win, const SDL_Point *area, void *data)
{
    if( *reinterpret_cast<bool*>(data) )
        return SDL_HITTEST_DRAGGABLE;

    return SDL_HITTEST_NORMAL;
}

//board width, height and square size are initialized with their setters later
ChessRenderer::ChessRenderer(NetworkEventSystem::Subscriber& networkEventSys, 
    BoardEventSystem::Subscriber& boardEventSubscriber, 
    GUIEventSystem::Publisher const& guiEventPublisher,
    AppEventSystem::Subscriber& appEventSubscriber) 

    : mGuiEventPublisher{guiEventPublisher}, 
      mNetworkSubManager{networkEventSys}, 
      mBoardSubscriber{boardEventSubscriber},
      mAppEventSubscriber{appEventSubscriber}
{
    //this is reset in dtor so that hitTestCallback can't be 
    //passed the dangling pointer to mIsHoveringDragableMenuBarRegion
    SDL_SetWindowHitTest(mWindow.window, &hitTestCallback, &mIsHoveringDragableMenuBarRegion);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");//bilinear texture filtering

    subToEvents();
    initSquareColorData();

    //instead of drawing to the whole screen, draw to the board texture.
    auto boardTex { mTextureManager.getTexture(TextureManager::WhichTexture::BOARD_TEXTURE).getTexture() };
    SDL_SetRenderTarget(mWindow.renderer, boardTex.get());
}

ChessRenderer::~ChessRenderer()
{
    SDL_SetWindowHitTest(mWindow.window, nullptr, nullptr);

    mBoardSubscriber.unsub<BoardEvents::GameOver>(mGameOverSubID);
    mBoardSubscriber.unsub<BoardEvents::PromotionBegin>(mPromotionBeginEventSubID);
    //mNetworkSubManager will automatically unsub from the rest of the events...

    serializeSquareColorData();
}

void ChessRenderer::serializeSquareColorData()
{
    SettingsManager squareColorDataManager {squareColorDataFname};

    if(auto maybeError {squareColorDataManager.setValue("L", getLightSquareColorAsString())} )
    {

        if(maybeError->code == SettingsManager::Error::Code::FILE_NOT_FOUND)
        {
            generateNewSquareColorDataTextFile(squareColorDataManager);
        }
        else
        {
            FileErrorLogger::get().log(maybeError->msg);
        }

        return;
    }

    if(auto maybeError {squareColorDataManager.setValue("D", getDarkSquareColorAsString())} )
    {
        if(maybeError->code != SettingsManager::Error::Code::FILE_NOT_FOUND)
            FileErrorLogger::get().log(maybeError->msg);
    }
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
    SettingsManager squareColorDataManager{squareColorDataFname};
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

std::string ChessRenderer::getLightSquareColorAsString()
{
    std::string lightSquareStr;
    for(auto const& val : mLightSquareColor)
        lightSquareStr.append(std::to_string(val)).append(" ");

    return lightSquareStr;
}

std::string ChessRenderer::getDarkSquareColorAsString()
{
    std::string darkSquareStr;
    for(auto const& val : mDarkSquareColor)
        darkSquareStr.append(std::to_string(val)).append(" ");

    return darkSquareStr;
}

void ChessRenderer::generateNewSquareColorDataTextFile(SettingsManager const& settingsManager)
{
    std::string const lightSquareStr { getLightSquareColorAsString() };
    std::string const darkSquareStr  { getDarkSquareColorAsString() };

    std::array<std::string, 3> const comments
    {
        "This is the RGBA color data (0 - 255) for the light squares (L) and the dark squares (d).",
        "If you accidentally mess this file up, just delete it and it will",
        "auto generate when you close the chess game next."
    };
    
    std::array const kvPairs
    {
        SettingsManager::KVPair{"L", lightSquareStr},
        SettingsManager::KVPair{"D", darkSquareStr}
    };
    
    settingsManager.generateNewFile(comments, kvPairs);
}

void ChessRenderer::drawSquares()
{
    SDL_Rect square{0, 0, static_cast<int>(mSquareSize), static_cast<int>(mSquareSize)};

    for(int i = 0; i < 8; ++i, square.x += mSquareSize)
    {
        for(int j = 0; j < 8; ++j, square.y += mSquareSize)
        {
            if( ! (i + j & 1) )//if light square
            {
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

        square.y = 0;
    }
}

void ChessRenderer::addRematchAndUnpairPopupButtons()
{
    mPopupManager.addButton({
        "Request rematch",
        [this]
        {
            GUIEvents::RematchRequest e;
            mGuiEventPublisher.pub(e);
            return true;
        }
    });

    mPopupManager.addButton({
        "Disconnect from opponent",
        [this]
        {
            GUIEvents::Unpair e;
            mGuiEventPublisher.pub(e);
            return true;
        }
    });
}

struct MenuBarStyles //RAII style push and pop imgui styles
{
    MenuBarStyles()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10, 4});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {.011f, 0.615f, 0.988f, .75f});
        ImGui::PushStyleColor(ImGuiCol_Separator, {0,0,0,1});
        ImGui::PushStyleColor(ImGuiCol_Button, {.2f, 0.79f, 0.8f, 0.70f});
        ImGui::PushStyleColor(ImGuiCol_Text, {0,0,0,1});
    }

    ~MenuBarStyles()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
};

static bool isPointInCircle(ImVec2 const p, ImVec2 const circleCenter, float const radius)
{
    ImVec2 const point2Circle { circleCenter.x - p.x , circleCenter.y - p.y };
    float const squaredDistance { point2Circle.x * point2Circle.x + point2Circle.y * point2Circle.y };
    return squaredDistance <= (radius * radius);
}

//returns true if the button was clicked
static bool closeButton(ImVec2 circleCenter, float const radius, bool& out_isHovered)
{
    bool const isMouseOverCloseButton { isPointInCircle(ImGui::GetMousePos(), circleCenter, radius) };
    auto* const wndDrawList { ImGui::GetWindowDrawList() };
 
    //draw the circle if the mouse is over the button
    if(isMouseOverCloseButton)
    {
        ImVec4 buttonColor { ImGui::GetStyle().Colors[ImGuiCol_Button] };

        if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
            buttonColor.w *= .6f; //reduce alpha value
        
        int const segmentCount { 36 };
        wndDrawList->AddCircleFilled(circleCenter, radius, ImGui::GetColorU32(buttonColor), segmentCount);

        //ImGui::GetWindowDrawList()->AddCircle(circleCenter, radius,
            //ImGui::GetColorU32({0,0,0,.6f}), segmentCount);
    }

    //draw the X
    float const crossSize { radius * .7071f * .8f };
    ImU32 crossColor { ImGui::GetColorU32({ 0, 0, 0, .7f }) };

    ImVec2 const bottomRight { circleCenter.x + crossSize,  circleCenter.y + crossSize };
    ImVec2 const topLeft     { circleCenter.x - crossSize,  circleCenter.y - crossSize };
    wndDrawList->AddLine(bottomRight, topLeft, crossColor, 1.3);

    ImVec2 const bottomLeft { circleCenter.x - crossSize,  circleCenter.y + crossSize };
    ImVec2 const topRight   { circleCenter.x + crossSize,  circleCenter.y - crossSize };
    wndDrawList->AddLine(bottomLeft, topRight, crossColor, 1.3);

    out_isHovered = isMouseOverCloseButton;
    return isMouseOverCloseButton && ImGui::GetIO().MouseReleased[ImGuiMouseButton_Left];
}

float ChessRenderer::drawMenuBar(Board const& b, ConnectionManager const& cm)
{
    //RAII style push and pop imgui styles
    [[maybe_unused]] MenuBarStyles styles;
    
    float menuBarHeight {0.0f};
    bool anyMenuBarButtonHovered {false};

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

        anyMenuBarButtonHovered = ImGui::IsItemHovered();//check if options tab is hovered

        if(ImGui::SmallButton("flip board"))
        {
            mViewingPerspective = mViewingPerspective == Side::WHITE ? Side::BLACK : Side::WHITE;
        }

        if( ! anyMenuBarButtonHovered )
            anyMenuBarButtonHovered = ImGui::IsItemHovered();//check if flip board button is hovered

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

                clearArrows();
            }
        }

        if( ! anyMenuBarButtonHovered )
            anyMenuBarButtonHovered = ImGui::IsItemHovered();//check if reset board button is hovered

        if(cm.isConnectedToServer())
        {
            if(cm.isPairedOnline())
            {
                if(ImGui::SmallButton("resign"))
                {
                    GUIEvents::Resign resignEvnt{};
                    mGuiEventPublisher.pub(resignEvnt);

                    mPopupManager.startNewPopup("You have resigned", false);
                    addRematchAndUnpairPopupButtons();
                }

                if( ! anyMenuBarButtonHovered )
                    anyMenuBarButtonHovered = ImGui::IsItemHovered();//check if hovering the resign button

                if(ImGui::SmallButton("draw"))
                {
                    GUIEvents::DrawOffer evnt{};
                    mGuiEventPublisher.pub(evnt);
                }

                if( ! anyMenuBarButtonHovered )
                    anyMenuBarButtonHovered = ImGui::IsItemHovered();//check if hovering the draw button
            }

            ImGui::Separator();
            ImGui::TextUnformatted("connected to server");
            ImGui::Separator();
            ImGui::Text("your ID: %u", cm.getUniqueID());
            ImGui::Separator();

            if(cm.isPairedOnline())
            {
                ImGui::Text("opponentID: %u", cm.getOpponentID());
                ImGui::Separator();
            }
        }
        else
        {
            ImGui::Separator();
            ImGui::TextUnformatted("not connected to server");
            ImGui::Separator();
        }

        auto whosTurn {b.getWhosTurnItIs() == Side::WHITE ? "it's white's turn" : "it's black's turn"};
        ImGui::TextUnformatted(whosTurn);

        menuBarHeight = ImGui::GetWindowHeight();

        float const closeBtnRadius { menuBarHeight * .4f };
        bool isCloseButtonHovered {false};
        if(closeButton({ImGui::GetWindowWidth() - closeBtnRadius - 4, menuBarHeight / 2}, menuBarHeight * .4f, isCloseButtonHovered))
        {
            GUIEvents::CloseButtonClicked evnt;
            mGuiEventPublisher.pub(evnt);
        }

        if(isCloseButtonHovered && ! anyMenuBarButtonHovered)
            anyMenuBarButtonHovered = true;

        mIsHoveringDragableMenuBarRegion = ! anyMenuBarButtonHovered && ImGui::IsWindowHovered();

        ImGui::EndMainMenuBar();
    }

    return menuBarHeight;
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

            //chess2ScreenPos takes into account where the board is on the imgui window.
            //when rendering to the SDL board texture 0,0 is the top left of the texture 
            //not the window, so subtract the board position
            screenPosition.x -= mBoardPos.x;
            screenPosition.y -= mBoardPos.y;

            auto const& texture { mTextureManager.getTexture(piece->getWhichTexture()) };

            //get the width and height of whichever texture the piece on the mouse is
            Vec2i textureSize { texture.getSize() };
            
            //from the screen position figure out the destination rectangle
            SDL_Rect destination
            {
                .x = screenPosition.x - static_cast<int>(textureSize.x * mBoardScalingFactor) / 2,
                .y = screenPosition.y - static_cast<int>(textureSize.y * mBoardScalingFactor) / 2,
                .w = static_cast<int>(textureSize.x * mBoardScalingFactor), 
                .h = static_cast<int>(textureSize.y * mBoardScalingFactor)
            };
            
            SDL_RenderCopy(mWindow.renderer, texture.getTexture().get(), nullptr, &destination);
        }
    }
}

void ChessRenderer::drawPieceOnMouse()
{
    auto const pom { Piece::getPieceOnMouse() };

    if(pom)
    {
        auto const& pieceTex { mTextureManager.getTexture(pom->getWhichTexture()) };
        auto const pieceTexSize { pieceTex.getSize() * mBoardScalingFactor };
        auto const texSizeHalfX = pieceTexSize.x / 2;
        auto const texSizeHalfY = pieceTexSize.y / 2;

        ImVec2 const localMousePos { getMousePosRelativeToMainImGuiWIndow() };
        ImVec2 const drawPos{localMousePos.x - texSizeHalfX, localMousePos.y - texSizeHalfY};

        ImGui::SetCursorPos(drawPos);
        ImGui::Image(pieceTex.getTexture().get(), pieceTexSize);
    }
}

void ChessRenderer::drawMoveIndicatorCircles(Board const& b)
{
    auto const pom = Piece::getPieceOnMouse();
    if( ! pom ) return;

    auto* const wdl { ImGui::GetWindowDrawList() };
    float const radius { mSquareSize / 6.0f };
    ImU32 const redColor  { ImGui::GetColorU32({.87f, .192f, .388f, .498f}) };
    ImU32 const greyColor { ImGui::GetColorU32( {.43f, .43f, .43f, .62f} ) };
    int const segmentCount { 40 };

    for(auto const& move : pom->getLegalMoves())
    {
        ImVec2 const circlePos { chess2ScreenPos(move.dest) };

        //If this legal move is a capture of another piece draw a red circle, otherwise draw a gray circle.
        if(move.wasCapture)
            wdl->AddCircleFilled(circlePos, radius, redColor, segmentCount);
        else 
            wdl->AddCircleFilled(circlePos, radius, greyColor, segmentCount);
    }
}

void ChessRenderer::renderToBoardTexture(Board const& b)
{
     auto boardTex { mTextureManager.getTexture(TextureManager::WhichTexture::BOARD_TEXTURE).getTexture() };
     SDL_SetRenderTarget(mWindow.renderer, boardTex.get());

     drawSquares();
     drawPiecesNotOnMouse(b);

     SDL_SetRenderTarget(mWindow.renderer, nullptr);
}

//is point in axis aligned rectangle
static bool isPointInRect(ImVec2 const& p, ImVec2 const& topLeft, ImVec2 const& bottomRight)
{
    return p.x > topLeft.x && p.y > topLeft.y && p.x < bottomRight.x && p.y < bottomRight.y; 
}

//takes a screen position on the board (assumed to be on the board) 
//and returns a vector in the middle of the square that it is in
Vec2i ChessRenderer::moveToMiddleOfSquare(Vec2i screenPosOnBoard)
{
    screenPosOnBoard -= mBoardPos;
    Vec2i const modValues { screenPosOnBoard % mSquareSize };

    screenPosOnBoard.x += (mSquareSize / 2) - modValues.x;
    screenPosOnBoard.y += (mSquareSize / 2) - modValues.y;
    
    return screenPosOnBoard += mBoardPos;
}

static ImVec2 rotatePointAroundOrigin(ImVec2 p, float radians)
{
    ImVec2 rotatedPos;
    float cosineOfAngle { std::cos(radians) }, sineOfAngle { std::sin(radians) };
    rotatedPos.x = (p.x * cosineOfAngle) - (p.y * sineOfAngle);
    rotatedPos.y = (p.x * sineOfAngle)   + (p.y * cosineOfAngle);
    return rotatedPos;
}

void ChessRenderer::drawArrow(ImVec2 const& arrowStart, ImVec2 const& arrowEnd, ImVec4 const& color)
{
    ImU32 u32Color {ImGui::GetColorU32(color)};
    ImVec2 arrowArm { arrowStart.x - arrowEnd.x,  arrowStart.y - arrowEnd.y };
    float const angleOfArrowArm { std::atan2(arrowArm.y, arrowArm.x) };
    float const isoscelesLength { mSquareSize * 0.4f };

    ImVec2 triVertex0 { rotatePointAroundOrigin({isoscelesLength, 0}, angleOfArrowArm + 0.5f) };
    ImVec2 triVertex1 { rotatePointAroundOrigin({isoscelesLength, 0}, angleOfArrowArm - 0.5f) };
    ImVec2 triangleBaseMidpoint { (triVertex1.x - triVertex0.x) / 2, (triVertex1.y - triVertex0.y) / 2 };

    triVertex0.x += arrowEnd.x;
    triVertex0.y += arrowEnd.y;
    triVertex1.x += arrowEnd.x;
    triVertex1.y += arrowEnd.y;
    triangleBaseMidpoint.x += triVertex0.x;
    triangleBaseMidpoint.y += triVertex0.y;

    auto* wdl { ImGui::GetWindowDrawList() };

    //draw the head of the arrow
    wdl->AddTriangle(triVertex0, triVertex1, arrowEnd, u32Color, 4);

    wdl->AddLine(arrowStart, triangleBaseMidpoint, u32Color, 10);
}

ImVec2 ChessRenderer::mainWindowDrawRankIndicators()
{
    float preRankIndicatorYPos { ImGui::GetCursorPosY() };
    
    char rankChar[2] {(Side::WHITE ==  mViewingPerspective) ? '8' : '1', '\0'};

    //draw dummy text (zero opacity) so I can query for the size of the font
    ImGui::TextColored({0,0,0,0}, rankChar);
    float const halfTextHeight { ImGui::GetItemRectSize().y / 2 };
    
    float spacing { (ImGui::GetStyle().WindowPadding.y + mSquareSize / 2) - halfTextHeight };
    ImGui::SetCursorPosY(spacing);
    for(int i = 0; i < 8; ++i)
    {
        ImGui::TextUnformatted(rankChar);

        rankChar[0] += (Side::WHITE == mViewingPerspective) ? -1 : 1;
        spacing += mSquareSize;
        ImGui::SetCursorPosY(spacing);
    }

    float textWidth { ImGui::GetItemRectMax().x };

    return {textWidth + ImGui::GetStyle().WindowPadding.x, preRankIndicatorYPos};
}

void ChessRenderer::mainWindowDrawFileIndicatiors()
{
    char fileChar[2] {'A', '\0'};

    float halfCharSize { ImGui::CalcTextSize(fileChar).x / 2 };
    float spacing {mBoardPos.x + (mSquareSize / 2.0f) - halfCharSize};
    ImGui::SetCursorPosX(spacing);

    for(int i = 0; i < 8; ++i)
    {
        ImGui::TextUnformatted(fileChar);

        ++fileChar[0];
        spacing += mSquareSize;
        ImGui::SameLine();
        ImGui::SetCursorPosX(spacing);
    }
}

void ChessRenderer::drawMainWindow(float const menuBarHeight, Board const& b)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    ImGui::SetNextWindowSize({static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)});
    ImGui::SetNextWindowPos({0.0f, menuBarHeight});

    auto const windowFlags
    {
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar
    };

    if(ImGui::Begin("##", nullptr, windowFlags))
    {
        mMainImGuiWindowPos = ImGui::GetWindowPos();

        //set the cursor position where the board texture should be drawn to
        ImGui::SetCursorPos(mainWindowDrawRankIndicators());

        //draw the board texture
        auto const& boardTex { mTextureManager.getTexture(TextureManager::WhichTexture::BOARD_TEXTURE) };
        ImGui::Image(boardTex.getTexture().get(), boardTex.getSize());

        mIsBoardHovered = ImGui::IsItemHovered();
        mBoardPos = ImGui::GetItemRectMin();
        ImVec2 boardBottomRight { ImGui::GetItemRectMax() };

        
        //ImGui::Text("%f, %f", ImGui::GetMousePos().x, ImGui::GetMousePos().y);

        mainWindowDrawFileIndicatiors();

        if( ! mIsPromotionWindowOpen ) [[likely]]
        {
            drawMoveIndicatorCircles(b);
            drawPieceOnMouse();
        }

        ImVec2 const lastRightClickScreenPos { ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Right] };
        bool const wasLastRightClickOnBoard { 
            isPointInRect(lastRightClickScreenPos, mBoardPos, boardBottomRight)
        };

        static bool wasDrawingArrowLastFrame {false};

        Vec2i lastRightClickMiddleSquarePos { moveToMiddleOfSquare(lastRightClickScreenPos) };
        Vec2i mousePosMiddleSquare { moveToMiddleOfSquare(ImGui::GetMousePos()) };

        bool const isPieceOnMouse { Piece::getPieceOnMouse() };

        if(ImGui::IsMouseDragging(ImGuiMouseButton_Right) && mIsBoardHovered 
            && wasLastRightClickOnBoard && ! isPieceOnMouse )
        {
            if(lastRightClickMiddleSquarePos != mousePosMiddleSquare)
            {
                drawArrow(lastRightClickMiddleSquarePos, mousePosMiddleSquare, mArrowColor);
                wasDrawingArrowLastFrame = true;
            }
        }

        //This will not trigger the same frame as we release right click. The next frame the static bool
        //will still be true though, and this will trigger. This is because
        //ImGui::IsMouseDragging and ImGui::isMouseReleased can't both be true durring the same frame
        if(wasDrawingArrowLastFrame && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            if(lastRightClickMiddleSquarePos != mousePosMiddleSquare && mIsBoardHovered && ! isPieceOnMouse )
                addArrow(lastRightClickMiddleSquarePos, mousePosMiddleSquare);

            //reset this to false so only 1 arrow gets added to the arrow buffer
            wasDrawingArrowLastFrame = false;
        }

        for(int i = 0; i < mArrowCount; ++i)
        {
            //when drawing arrows that are already set, lower their opacity
            ImVec4 color {mArrowColor};
            color.w *= .4f;

            drawArrow(mArrowBuffer[i].arrowBasePos, mArrowBuffer[i].arrowHeadPos, color);
        }
           
        ImGui::End();
    }

    ImGui::PopStyleVar();
}

void ChessRenderer::render(Board const& b, ConnectionManager const& cm)
{
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    mPopupManager.draw();

    float const menuBarHeight { drawMenuBar(b, cm) };
    drawMainWindow(menuBarHeight, b);

    if(mIsColorEditorWindowOpen) [[unlikely]]
        drawColorEditor();

    if(mIsConnectionWindowOpen)  [[unlikely]]
        drawConnectionWindow();

    if(mIsPromotionWindowOpen)   [[unlikely]]
        drawPromotionWindow();

    renderToBoardTexture(b);

    ImGui::ShowDemoWindow();
    //ImGui::ShowMetricsWindow();

    ImGui::Render();
    SDL_SetRenderDrawColor(mWindow.renderer, 0, 0, 0, 0);
    SDL_RenderClear(mWindow.renderer);
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

void ChessRenderer::drawPromotionWindow()
{
    PromotionImguiStyles raiiStyles;
    
    ImGui::Begin("pick a piece!", nullptr, ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBackground);

    Vec2i promoScreenPos { chess2ScreenPos(mPromotionContext.promotingSquare) };
    promoScreenPos.x -= mSquareSize / 2;
    promoScreenPos.y -= mSquareSize / 2;

    Side const promoSide {mPromotionContext.promotingSide}; //shorter name

    if(promoSide != mViewingPerspective)
        promoScreenPos.y -= mSquareSize * 3;
        
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

    if(ImGui::SmallButton("reset light squares"))
        for(int i = 0; i < 4; ++i)
            f_lightSquares[i] = mDefaultLightSquareColor[i] / 255.0f;

    ImGui::Separator();

    ImGui::TextUnformatted("reset dark squares");
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
        std::format(
            "The ID you supplied ({}) was not in the server lobby (or it is your ID).", evnt.ID
        ), 
        true
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

            mPopupManager.startNewPopup("you have accepted the draw", false);
            addRematchAndUnpairPopupButtons();

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
            mPopupManager.startNewPopup("you have accepted the rematch. Time to play again!", true);
            GUIEvents::RematchAccept evnt{};
            mGuiEventPublisher.pub(evnt);
            return true;
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
    addRematchAndUnpairPopupButtons();
}

void ChessRenderer::onGameOverEventWhileNotPaired(BoardEvents::GameOver const& evnt)
{
    mPopupManager.startNewPopup(evnt.reason, true);
}

void ChessRenderer::onUnpairEvent()
{
    mPopupManager.startNewPopup("You have been unpaired with your opponent and put back into the lobby", true);

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

void ChessRenderer::onPairRequestWhilePairedEvent()
{
    mIsConnectionWindowOpen = false;

    mPopupManager.startNewPopup(
        "you can't connect to another player while paired with an opponent", true
    );
}

void ChessRenderer::onRematchAcceptEvent()
{
    mPopupManager.startNewPopup("your opponent accepted your rematch request!", true);
}

void ChessRenderer::onOpponentHasResignedEvent()
{
    mPopupManager.startNewPopup("your opponent has resigned", false);
    addRematchAndUnpairPopupButtons();
}

void ChessRenderer::onDrawAcceptedEvent()
{
    mPopupManager.startNewPopup("your opponent has accepted your draw offer", false);
    addRematchAndUnpairPopupButtons();
}

void ChessRenderer::onLeftClickEvent(AppEvents::LeftClickPress const& evnt)
{
    //was the left click on the board
    if(auto maybeChessPos {screen2ChessPos(evnt.mousePos)})
    {
        GUIEvents::PiecePickUp evnt {*maybeChessPos};
        mGuiEventPublisher.pub(evnt);

        clearArrows();
    }
}

void ChessRenderer::subToEvents()
{
    mLeftClickEventSubID = mAppEventSubscriber.sub<AppEvents::LeftClickPress>(
        [this](Event const& e){ onLeftClickEvent(e.unpack<AppEvents::LeftClickPress>()); });

    mNetworkSubManager.sub<NetworkEvents::DrawAccept>(NetworkSubscriptions::DRAW_ACCEPTED,
        [this](Event const&){ onDrawAcceptedEvent(); });

    mNetworkSubManager.sub<NetworkEvents::OpponentHasResigned>(NetworkSubscriptions::OPPONENT_RESIGNED,
        [this](Event const&){ onOpponentHasResignedEvent(); });

    mNetworkSubManager.sub<NetworkEvents::PairRequestWhilePaired>(NetworkSubscriptions::PAIR_REQUEST_WHILE_PAIRED,
        [this](Event const&){ onPairRequestWhilePairedEvent(); });

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

    mNetworkSubManager.sub<NetworkEvents::RematchAccept>(NetworkSubscriptions::REMATCH_ACCEPT,
        [this](Event const&){ onRematchAcceptEvent(); });

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

    mPromotionBeginEventSubID = mBoardSubscriber.sub<BoardEvents::PromotionBegin>([this](Event const& e){
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
    ret.x *= mSquareSize;
    ret.y *= mSquareSize;

    //move from top left to middle of square
    ret.y += static_cast<int>(mSquareSize / 2);
    ret.x += static_cast<int>(mSquareSize / 2);

    //account for where the board location offset in the window
    ret.x += mBoardPos.x;
    ret.y += mBoardPos.y;

    return ret;
}

//returns where the mouse pos is relative to the main imgui
//window (the one where the board is drawn)
Vec2i ChessRenderer::getMousePosRelativeToMainImGuiWIndow()
{
    Vec2i const globalMousePos { ImGui::GetMousePos() };
    return globalMousePos - mMainImGuiWindowPos;
}

void ChessRenderer::addArrow(Vec2i const& arrowBasePos, Vec2i const& arrowHeadPos)
{
    if(mArrowCount == mMaxArrows)
        return;

    mArrowBuffer[mArrowCount] = {arrowBasePos, arrowHeadPos};
    ++mArrowCount;
}

void ChessRenderer::clearArrows()
{
    mArrowCount = 0;
}

//if the input coords on on the board, then return the corresponding chess square
std::optional<Vec2i> ChessRenderer::screen2ChessPos(Vec2i const pos) const
{
    if( ! mIsBoardHovered )
        return std::nullopt;

    Vec2i chessPos { (pos.x - mBoardPos.x) / mSquareSize, (pos.y - mBoardPos.y) / mSquareSize };

    if(Side::WHITE == mViewingPerspective) { chessPos.y = 7 - chessPos.y; }
    else { chessPos.x = 7 - chessPos.x; }

    return std::make_optional(chessPos);
}