#include <cstdint>
#include <array>
#include <vector>
#include <sstream>
#include <cassert>
#include <functional>//std::invoke

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "SDL.h"
#include "SDL_image.h"
#include "imgui.h"
#include "chessNetworkProtocol.h"
#include "ChessApplication.h"
#include "PieceTypes.h"
#include "Vector2i.h"
#include "errorLogger.h"

#define SQUARE_COLOR_DATA_FILENAME "squareColorData.txt"

//this module is for drawing the necessary stuff
//with SDL and imgui for the chess application

//board width, height and square size are initialized with their setters later
ChessDrawer::ChessDrawer(uint32_t squareSize)
    : m_squareSize{squareSize}, m_chessBoardWidth{squareSize * 8}, m_chessBoardHeight{squareSize * 8},
      m_menuBarSize{0.0f, 0.0f}, m_pieceTextures{}, m_pieceTextureSizes{},
      m_lightSquareColor{214, 235, 225, 255}, m_darkSquareColor{43, 86, 65, 255},
      m_circleTexture{nullptr}, m_redCircleTexture{nullptr}
{
    //important that they are in this order, as some optimizations are done later that assume
    //that m_pieceTextures are in the same order is ChessDrawer::TextureIndicies
    loadPieceTexturesFromDisk
    ({
        "textures/wPawn.png",
        "textures/wKnight.png",
        "textures/wRook.png",
        "textures/wBishop.png",
        "textures/wQueen.png",
        "textures/wKing.png",
        "textures/bPawn.png",
        "textures/bKnight.png",
        "textures/bRook.png",
        "textures/bBishop.png",
        "textures/bQueen.png",
        "textures/bKing.png"
    });

    initCircleTexture(m_squareSize / 6, 0x6F, 0x6F, 0x6F, 0x9F, &m_circleTexture);
    initCircleTexture(m_squareSize / 6, 0xDE, 0x31, 0x63, 0x7F, &m_redCircleTexture);

    ImGui::StyleColorsLight();

    m_windowFunctionMap[WindowTypes::COLOR_EDITOR]      = {&ChessDrawer::drawColorEditorWindow, false};
    m_windowFunctionMap[WindowTypes::CONNECTION]        = {&ChessDrawer::drawConnectionWindow, false};
    m_windowFunctionMap[WindowTypes::DRAW_DECLINED]     = {&ChessDrawer::drawDrawDeclinedPopup, false};
    m_windowFunctionMap[WindowTypes::DRAW_OFFER]        = {&ChessDrawer::drawDrawOfferPopup, false};
    m_windowFunctionMap[WindowTypes::ID_NOT_IN_LOBBY]   = {&ChessDrawer::drawIDNotInLobbyPopup, false};
    m_windowFunctionMap[WindowTypes::PAIRING_COMPLETE]  = {&ChessDrawer::drawPairingCompletePopup, false};
    m_windowFunctionMap[WindowTypes::PAIRING_DECLINED]  = {&ChessDrawer::drawPairingDeclinedPopup, false};
    m_windowFunctionMap[WindowTypes::PAIR_REQUEST]      = {&ChessDrawer::drawPairRequestPopup, false};
    m_windowFunctionMap[WindowTypes::PROMOTION]         = {&ChessDrawer::drawPromotionPopup, false};
    m_windowFunctionMap[WindowTypes::REMATCH_REQUEST]   = {&ChessDrawer::drawRematchRequestPopup, false};
    m_windowFunctionMap[WindowTypes::RESET_BOARD_ERROR] = {&ChessDrawer::drawResetBoardErrorPopup, false};
    m_windowFunctionMap[WindowTypes::UNPAIR]            = {&ChessDrawer::drawUnpairPopup, false};
    m_windowFunctionMap[WindowTypes::WIN_LOSS_DRAW]     = {&ChessDrawer::drawWinLossDrawPopup, false};
}

ChessDrawer::~ChessDrawer()
{
    for(auto t : m_pieceTextures) SDL_DestroyTexture(t);
    SDL_DestroyTexture(m_circleTexture);
    SDL_DestroyTexture(m_redCircleTexture);

    serializeSquareColorData();
}

//generates a circle texture at startup to use later
void ChessDrawer::initCircleTexture(int radius, Uint8 RR, Uint8 GG, 
    Uint8 BB, Uint8 AA, SDL_Texture** toInit)
{
//make sure the ordering of the RGBA bytes will be the same
//regarless of endianess of the target platform
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 rMask = 0xFF000000;
    Uint32 gMask = 0x00FF0000;
    Uint32 bMask = 0x0000FF00;
    Uint32 aMask = 0x000000FF;
    Uint32 circleColor = (RR << 24) + (GG << 16) + (BB << 8) + AA;
#else
    uint32_t rMask = 0x000000FF;
    uint32_t gMask = 0x0000FF00;
    uint32_t bMask = 0x00FF0000;
    uint32_t aMask = 0xFF000000;
    uint32_t circleColor = (AA << 24) + (BB << 16) + (GG << 8) + RR;
#endif
    int const diameter = radius * 2;
    SDL_Rect const boundingBox{-radius, -radius, diameter, diameter};
    uint32_t const pixelCount = diameter * diameter;
    auto pixels = std::make_unique<uint32_t[]>(pixelCount);

    int radiusSquared = radius * radius;
    int xOffset = -radius, yOffset = -radius;
    for(int x = 0; x < diameter; ++x)
    {
        for(int y = 0; y < diameter; ++y)
        {
            if((x-radius)*(x-radius) + (y-radius)*(y-radius) <= radiusSquared)
            {
                pixels[x + y * diameter] = circleColor;
            }
        }
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom
    (
         pixels.get(), 
         diameter, 
         diameter,
         32, 
         diameter * sizeof(uint32_t), 
         rMask, gMask, bMask, aMask
    );
   
    if(!surface) throw std::exception(SDL_GetError());

    *toInit = SDL_CreateTextureFromSurface(ChessApp::getApp().getCurrentRenderer(), surface);
    SDL_FreeSurface(surface);
}

void ChessDrawer::drawSquares()
{
    const auto& app = ChessApp::getApp();

    SDL_Rect square{0, static_cast<int>(m_menuBarSize.y),
        static_cast<int>(m_squareSize), static_cast<int>(m_squareSize)};

    SDL_Renderer *const renderer = app.getCurrentRenderer();

    for(int i = 0; i < 8; ++i, square.x += m_squareSize)
    {
        for(int j = 0; j < 8; ++j, square.y += m_squareSize)
        {
            if( !( i + j & 1 ) )//if light square
            {
                //scale up the colors from 0-1 to 0-255 and draw
                SDL_SetRenderDrawColor
                (
                    renderer,
                    m_lightSquareColor[0],
                    m_lightSquareColor[1],
                    m_lightSquareColor[2],
                    m_lightSquareColor[3]
                );
            }
            else//if dark square
            {
                SDL_SetRenderDrawColor
                (
                    renderer,
                    m_darkSquareColor[0],
                    m_darkSquareColor[1],
                    m_darkSquareColor[2],
                    m_darkSquareColor[3]
                );
            }

            SDL_RenderFillRect(renderer, &square);
        }

        square.y = static_cast<int>(m_menuBarSize.y);
    }
}

void ChessDrawer::drawIDNotInLobbyPopup()
{
    auto& app = ChessApp::getApp();
    assert(app.getNetWork().isThereAPotentialOpponent());
    
    centerNextWindow();
    ImGui::OpenPopup("Invalid ID");
    if(ImGui::BeginPopup("Invalid ID"))
    {
        uint32_t potentialOpponentIDStr = app.getNetWork().getPotentialOpponentsID();
        ImGui::Text("The given ID (%u)\n"
            "is either not connected to the server,\n"
            "or you gave your own ID.", potentialOpponentIDStr);

        if(ImGui::Button("okay"))
        {
            app.getNetWork().setIsThereAPotentialOpponent(false);

            closeWindow(WindowTypes::ID_NOT_IN_LOBBY);
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void ChessDrawer::drawPiecesNotOnMouse()
{
    auto const& pom = Piece::getPieceOnMouse();
    auto const& pieces = ChessApp::getBoard().getPieces();

    for(auto const& piece : pieces)
    {
        if(piece && piece != pom)
        {
            //figure out where on the screen the piece is (the middle of the square)
            Vec2i screenPosition = ChessApp::chess2ScreenPos(piece->getChessPosition());
            
            //get the index into the array of piece textures for this piece
            auto index = piece->getWhichTexture();

            //get the width and height of whichever texture the piece on the mouse is
            Vec2i textureSize = m_pieceTextureSizes[index];
            
            //from the screen position figure out the destination rectangle
            SDL_Rect destination
            {
                .x = screenPosition.x - textureSize.x / 2,
                .y = screenPosition.y - textureSize.y / 2,
                .w = textureSize.x, 
                .h = textureSize.y
            };
            
            SDL_RenderCopy
            (
                ChessApp::getApp().getCurrentRenderer(),
                m_pieceTextures[index],
                nullptr, 
                &destination
            );
        }
    }
}

void ChessDrawer::drawPieceOnMouse()
{
    auto const& renderer = ChessApp::getApp().getCurrentRenderer();
    auto const pom = Piece::getPieceOnMouse();

    if(pom)
    {
        Vec2i mousePosition{};
        SDL_GetMouseState(&mousePosition.x, &mousePosition.y);
        auto textureSize = m_pieceTextureSizes[pom->getWhichTexture()];
        SDL_Rect destination
        {
            .x = mousePosition.x - textureSize.x / 2,
            .y = mousePosition.y - textureSize.y / 2,
            .w = textureSize.x, 
            .h = textureSize.y
        };
        SDL_RenderCopy
        (
            renderer,
            m_pieceTextures[pom->getWhichTexture()],
            nullptr,
            &destination
        );
    }
}

void ChessDrawer::drawMoveIndicatorCircles()
{
    auto const pom = Piece::getPieceOnMouse();
    if(!pom) return;

    auto const& app = ChessApp::getApp();
    SDL_Renderer *const renderer  = app.getCurrentRenderer();
    auto const& board = app.getBoard();

    SDL_Rect circleSource{};
    SDL_QueryTexture(m_circleTexture, nullptr, nullptr, &circleSource.w, &circleSource.h);

    for(auto const& move : pom->getLegalMoves())
    {
        Vec2i const circlePos{ChessApp::chess2ScreenPos(move.m_dest)};
        SDL_Rect const circleDest
        {
            static_cast<int>(circlePos.x - circleSource.w * 0.5f),
            static_cast<int>(circlePos.y - circleSource.w * 0.5f),
            static_cast<int>(circleSource.w),
            static_cast<int>(circleSource.h)
        };

        //if there is an enemy piece (or enPassant square being targeted by a pawn)
        //then draw red circle instead of the usual grey circle
        auto const piece = board.getPieceAt(move.m_dest);
        if(piece && piece->getSide() != board.getWhosTurnItIs() ||
            (move.m_dest == board.getEnPassantIndex()) && std::dynamic_pointer_cast<Pawn>(pom))
        {
            SDL_RenderCopy(renderer, m_redCircleTexture, &circleSource, &circleDest);
            continue;
        }

        SDL_RenderCopy(renderer, m_circleTexture, &circleSource, &circleDest);
    }
}

void ChessDrawer::drawDrawDeclinedPopup()
{
    centerNextWindow();
    ImGui::OpenPopup("draw declined");
    if(ImGui::BeginPopup("draw declined", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextUnformatted("opponent declined your draw offer.");
        if(ImGui::Button("okay"))
        {
            closeWindow(WindowTypes::DRAW_DECLINED);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ChessDrawer::openWindow(WindowTypes whichWindow)
{
    auto it = m_windowFunctionMap.find(whichWindow);
    assert(it != m_windowFunctionMap.end());

    auto& [window, isOpen] = it->second;
    if(!isOpen)//if the window is closed
    {
        //then open the window
        m_openWindows.push_back({window, false});
        isOpen = true;
    }
}

void ChessDrawer::closeWindow(WindowTypes whichWindow)
{
    auto i = m_windowFunctionMap.find(whichWindow);
    assert(i != m_windowFunctionMap.end());

    auto& [window, isOpen] = i->second;
    if(isOpen)//if the window is open
    {
        //Linear search is fast here, since there is only ever a few (at most) windows open.
        auto const it = std::find_if(m_openWindows.begin(), m_openWindows.end(), 
            [window](auto const& elem){return elem.first == window;});

        assert(it != m_openWindows.end());

        it->second = true;//This bool is a flag for defered deletion from the vector. 
        isOpen = false;
    }
}

void ChessDrawer::renderAllTheThings()
{
    deferedWindowDeletion();
    auto const renderer = ChessApp::getApp().getCurrentRenderer();
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    for(auto& window : m_openWindows)
        std::invoke(window.first, this);

    if(m_imguiDemoWindowIsOpen) [[unlikely]]
        ImGui::ShowDemoWindow();

    drawMenuBar();

    ImGui::Render();
    SDL_RenderClear(renderer);
    drawSquares();
    drawPiecesNotOnMouse();
    if(!isPromotionWindowOpen()) [[likely]]
    {
        drawMoveIndicatorCircles();
        drawPieceOnMouse();
    }
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer);
}

bool ChessDrawer::isPromotionWindowOpen() const
{
    auto it = m_windowFunctionMap.find(WindowTypes::PROMOTION);
    assert(it != m_windowFunctionMap.end());
    return it->second.second;
}

void ChessDrawer::deferedWindowDeletion()
{
    std::erase_if(m_openWindows, [](auto const& pair){return pair.second;});
}

void ChessDrawer::drawColorEditorWindow()
{
    static bool isOpen {true};

    ImGui::Begin("change square colors", &isOpen,
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::TextUnformatted("light squares");
    
    ImVec4 f_lightSquares{};//a float (0-1) version of the light squares
    ImVec4 f_darkSquares{};//a float (0-1) version of the dark squares
    for(int i = 0; i < 4; ++i)
    {
        f_lightSquares[i] = m_lightSquareColor[i] / 255.0f;
        f_darkSquares[i]  = m_darkSquareColor[i]  / 255.0f;
    }
    
    ImGui::ColorPicker3("light squares", &f_lightSquares[0]);
    ImGui::Separator();
    ImGui::TextUnformatted("dark squares");
    ImGui::ColorPicker3("dark squares", &f_darkSquares[0]);
    
    for(int i = 0; i < 4; ++i)
    {
        m_lightSquareColor[i] = static_cast<Uint8>(f_lightSquares[i] * 255);
        m_darkSquareColor[i] = static_cast<Uint8>(f_darkSquares[i] * 255);
    }
    
    if(!isOpen) 
    {
        closeWindow(WindowTypes::COLOR_EDITOR); 
        isOpen = true;
    }

    ImGui::End();
}

void ChessDrawer::drawConnectionWindow()
{
    static bool isOpen{true};
    ImGui::Begin("connect to another player", &isOpen,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    
    static bool isInputValid{true};

    //The ID's are uint32_t, so we need at most 10 chars plus '\0'.
    char opponentID[11] = {0};
    
    ImGui::TextUnformatted("Enter the ID of the player you wish to play against.");
    ImGui::TextUnformatted("Your ID is at the top of the window in the title bar.");
    if(ImGui::InputText("<- opponent's ID", opponentID, sizeof(opponentID), 
        ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if(ChessConnection::isOpponentIDStringValid(opponentID))
        {
            ChessApp::getApp().buildAndSendPairRequest(opponentID);
            isInputValid = true;
        }
        else
        {
            isInputValid = false;
        }        
    }

    if(!isInputValid)
        ImGui::TextUnformatted("The given ID is invalid!");

    if(!isOpen)
    {
        closeWindow(WindowTypes::CONNECTION);
        isOpen = true;
    }

    ImGui::End();
}

void ChessDrawer::drawPromotionPopup()
{
    auto& app = ChessApp::getApp();
    auto& board = app.getBoard();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {1.0f, 1.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   {0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  {0.0f, 1.0f});
    
    ImGui::Begin("pick a piece!", nullptr,
        ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove      |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse  |
        ImGuiWindowFlags_NoTitleBar  |
        ImGuiWindowFlags_NoBackground);

    bool wasPromoACapture = !!board.getLastCapturedPiece();
    Move promoMove = board.getLastMove();//get the move made by the pawn to promote
    assert(promoMove.m_moveType == MoveInfo::PROMOTION || 
        promoMove.m_moveType == MoveInfo::ROOK_CAPTURE_AND_PROMOTION);
    Vec2i promotionSquare = promoMove.m_dest;
    Vec2i promotionScreenPos = ChessApp::chess2ScreenPos(promotionSquare);
    promotionScreenPos.x -= m_squareSize * 0.5f; promotionScreenPos.y -= m_squareSize * 0.5f;
    if(board.getViewingPerspective() != board.getWhosTurnItIs())
        promotionScreenPos.y -= m_squareSize * 3;

    ImGui::SetWindowPos(promotionScreenPos);

    auto indecies = (board.getWhosTurnItIs() == Side::WHITE) ? std::array{WQUEEN, WROOK, WKNIGHT, WBISHOP} :
        std::array{BQUEEN, BROOK, BKNIGHT, BBISHOP};

    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[0]]), m_pieceTextureSizes[indecies[0]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        board.postMoveUpdate(promoMove, PromoType::QUEEN_PROMOTION); 
        closeWindow(WindowTypes::PROMOTION);
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[1]]), m_pieceTextureSizes[indecies[1]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        board.postMoveUpdate(promoMove, PromoType::ROOK_PROMOTION);
        closeWindow(WindowTypes::PROMOTION);
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[2]]), m_pieceTextureSizes[indecies[2]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        board.postMoveUpdate(promoMove, PromoType::KNIGHT_PROMOTION);
        closeWindow(WindowTypes::PROMOTION);
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[3]]), m_pieceTextureSizes[indecies[3]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        board.postMoveUpdate(promoMove, PromoType::BISHOP_PROMOTION);
        closeWindow(WindowTypes::PROMOTION);
    }

    ImGui::PopStyleVar(4);
    ImGui::End();
}

void ChessDrawer::drawDrawOfferPopup()
{
    auto& app = ChessApp::getApp();

    centerNextWindow();
    ImGui::OpenPopup("opponent has offered a draw");
    if(ImGui::BeginPopup("opponent has offered a draw"))
    {
        ImGui::TextUnformatted("Your opponent has offered a draw.");

        if(ImGui::Button("Accept"))
        {
            app.sendHeaderOnlyMessage(MessageType::DRAW_ACCEPT_MSGTYPE, 
                MessageSize::DRAW_ACCEPT_MSGSIZE);
            app.updateGameState(GameState::DRAW_AGREEMENT);
            openWindow(WindowTypes::WIN_LOSS_DRAW);
            closeWindow(WindowTypes::DRAW_OFFER);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if(ImGui::Button("Decline"))
        {
            app.sendHeaderOnlyMessage(MessageType::DRAW_DECLINE_MSGTYPE,
                MessageSize::DRAW_DECLINE_MSGSIZE);
            closeWindow(WindowTypes::DRAW_OFFER);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ChessDrawer::pushMenuBarStyles()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {9,5});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {.011f, 0.615f, 0.988f, .75f});
    ImGui::PushStyleColor(ImGuiCol_Separator, {0,0,0,1});
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, {183/255.f, 189/255.f, 188/255.f, 1});
    ImGui::PushStyleColor(ImGuiCol_Button, {.2f, 0.79f, 0.8f, 0.70f});
}

void ChessDrawer::popMenuBarStyles()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void ChessDrawer::centerNextWindow()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

void ChessDrawer::drawMenuBar()
{
    auto& app = ChessApp::getApp();
    auto& board = app.getBoard();

    pushMenuBarStyles();

    if(ImGui::BeginMainMenuBar()) [[unlikely]]
    {
        if(ImGui::BeginMenu("options"))
        {
            if(ImGui::MenuItem("change square colors", nullptr, nullptr))
                openWindow(WindowTypes::COLOR_EDITOR);

            if(ImGui::MenuItem("connect to another player", nullptr, nullptr))
                openWindow(WindowTypes::CONNECTION);

            ImGui::EndMenu();
        }

        if(ImGui::SmallButton("flip board"))
            board.flipBoardViewingPerspective();
    
        if(ImGui::SmallButton("reset board"))
        {
            if(!app.isPairedWithOpponent()) board.resetBoard();
            else openWindow(WindowTypes::RESET_BOARD_ERROR);
        }

        if(app.isConnectedToServer())
        {
            if(app.isPairedWithOpponent())
            {
                if(ImGui::SmallButton("resign"))
                {
                    app.sendHeaderOnlyMessage(MessageType::RESIGN_MSGTYPE, 
                        MessageSize::RESIGN_MSGSIZE);
                    app.updateGameState(GameState::YOU_RESIGNED);
                    openWindow(WindowTypes::WIN_LOSS_DRAW);
                }

                if(ImGui::SmallButton("draw"))
                {
                    app.sendHeaderOnlyMessage(MessageType::DRAW_OFFER_MSGTYPE,
                        MessageSize::DRAW_OFFER_MSGSIZE);
                }
            }

            ImGui::Separator();
            ImGui::Text("connected to server");
            ImGui::Separator();
            ImGui::Text("your ID: %u", app.getNetWork().getUniqueID());
            ImGui::Separator();

            if(app.isPairedWithOpponent())
                ImGui::Text("opponentID: %u", app.getNetWork().getOpponentID());
        }
        else 
        {
            ImGui::Separator();
            ImGui::TextUnformatted("not connected to server");
            ImGui::Separator();
        }

        char const* whosTurn = board.getWhosTurnItIs() == Side::WHITE ?
            "it's white's turn" : "it's black's turn";
        ImGui::SameLine(app.getWindowWidth() - (ImGui::CalcTextSize(whosTurn).x + 18));
        ImGui::TextUnformatted(whosTurn);

        static bool firstPass = true;
        if(firstPass) [[unlikely]]
        {
            m_menuBarSize = ImGui::GetWindowSize();
            SDL_SetWindowSize(app.getSDLWindow(), app.getWindowWidth(), 
                app.getWindowHeight() + m_menuBarSize.y);
        }

        ImGui::EndMainMenuBar();
    }

    popMenuBarStyles();
}

void ChessDrawer::drawResetBoardErrorPopup()
{
    ImGui::OpenPopup("error");

    //Center the next window.
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::BeginPopup("error", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextUnformatted("can't reset board when connected with another player!");
        if(ImGui::Button("okay"))
        {
            ImGui::CloseCurrentPopup();
            closeWindow(WindowTypes::RESET_BOARD_ERROR);
        }
        ImGui::EndPopup();
    }
}

void ChessDrawer::drawPairingCompletePopup()
{
    ImGui::OpenPopup("successfully connected");

    auto& network = ChessApp::getNetWork();
    auto& board = ChessApp::getBoard();

    //Center the next window.
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::BeginPopup("successfully connected"))
    {
        auto opponentStr = std::to_string(network.getPotentialOpponentsID());
        std::string whichPieces("you are playing with the ");
        whichPieces.append(board.getSideUserIsPlayingAs() ==
            Side::WHITE ? "white " : "black ");
        whichPieces.append("pieces");

        ImGui::TextUnformatted("playing against user with ID ");
        ImGui::SameLine();
        ImGui::TextUnformatted(opponentStr.data());
        ImGui::TextUnformatted(whichPieces.data());

        if(ImGui::Button("lets play!"))
        {
            ImGui::CloseCurrentPopup();
            closeWindow(WindowTypes::PAIRING_COMPLETE);
        }

        ImGui::EndPopup();
    }
}

void ChessDrawer::drawWinLossDrawPopup()
{
    auto& app = ChessApp::getApp();
    auto& board = app.getBoard();

    assert(app.getGameState() != GameState::GAME_IN_PROGRESS);

    centerNextWindow();
    ImGui::OpenPopup("game over");
    if(ImGui::BeginPopup("game over"))
    {
        ImGui::TextUnformatted(m_winLossDrawPopupMessage.data());
        
        if(app.isPairedWithOpponent())
        {
            if(ImGui::Button("request rematch"))
            {
                app.sendHeaderOnlyMessage(MessageType::REMATCH_REQUEST_MSGTYPE, 
                    MessageSize::REMATCH_REQUEST_MSGSIZE);
            }
            if(ImGui::Button("disconnect from opponent"))
            {
                app.sendHeaderOnlyMessage(MessageType::UNPAIR_MSGTYPE, 
                    MessageSize::UNPAIR_MSGSIZE);
                app.getNetWork().setIsPairedWithOpponent(false);
                closeWindow(WindowTypes::UNPAIR);
                board.resetBoard();
            }
        }
        else
        {
            if(ImGui::Button("okay"))
                board.resetBoard();
        }

        ImGui::EndPopup();
    } 
}

void ChessDrawer::drawRematchRequestPopup()
{
    auto& app = ChessApp::getApp();
    auto& board = app.getBoard();

    centerNextWindow();
    ImGui::OpenPopup("Opponent has requested rematch.");
    if(ImGui::BeginPopup("Opponent has requested rematch."))
    {
        ImGui::Text("Your opponent has offered a rematch");

        if(ImGui::Button("accept rematch"))
        {
            board.resetBoard();
            app.sendHeaderOnlyMessage(MessageType::REMATCH_ACCEPT_MSGTYPE,
                MessageSize::REMATCH_ACCEPT_MSGSIZE);
            closeWindow(WindowTypes::REMATCH_REQUEST);
        }
        
        //decline and disconnect from opponent
        if(ImGui::Button("decline rematch"))
        {
            app.sendHeaderOnlyMessage(MessageType::REMATCH_DECLINE_MSGTYPE,
                MessageSize::REMATCH_DECLINE_MSGSIZE);
            board.resetBoard();
            app.getNetWork().setIsPairedWithOpponent(false);
            closeWindow(WindowTypes::REMATCH_REQUEST);
            openWindow(WindowTypes::UNPAIR);
        }

        ImGui::EndPopup();
    }
}

void ChessDrawer::drawPairRequestPopup()
{
    auto& app = ChessApp::getApp();

    centerNextWindow();
    ImGui::OpenPopup("someone wants to play chesssss");
    if(ImGui::BeginPopup("someone wants to play chesssss"))
    {
        uint32_t potentialOpponentID = app.getNetWork().getPotentialOpponentsID();
        ImGui::Text("request from ID %u to play chess!", potentialOpponentID);
        if(ImGui::Button("accept"))
        {
            closeWindow(WindowTypes::PAIR_REQUEST);
            ImGui::CloseCurrentPopup();
            app.buildAndSendPairAccept();
        }
        if(ImGui::Button("decline")) 
        {
            closeWindow(WindowTypes::PAIR_REQUEST);
            ImGui::CloseCurrentPopup();
            app.buildAndSendPairDecline();
        }
        ImGui::EndPopup();
    }
}

void ChessDrawer::drawPairingDeclinedPopup()
{
    centerNextWindow();
    ImGui::OpenPopup("pair request declined");
    if(ImGui::BeginPopup("pair request declined"))
    {
        ImGui::Text("Your offer to play chess was declined");
        
        if(ImGui::Button("okay"))
        {
            closeWindow(WindowTypes::PAIRING_DECLINED);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ChessDrawer::drawUnpairPopup()
{
    ImGui::OpenPopup("unpair");
    centerNextWindow();
    if(ImGui::BeginPopup("unpair"))
    {
        ImGui::TextUnformatted("You were disconnected and put back in the server lobby");

        if(ImGui::Button("okay"))
        {
            closeWindow(WindowTypes::UNPAIR);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

//called inside of openWinLossDrawPopup()
//in order to update m_winLossDrawPopupMessage before drawing the win loss draw popup
void ChessDrawer::updateWinLossDrawMessage()
{
    using enum GameState;
    auto& app = ChessApp::getApp();
    auto& board = app.getBoard();
    auto gs = app.getGameState();
    assert(gs != GAME_IN_PROGRESS);
    auto whosTurnIsIt = board.getWhosTurnItIs();

    //if game was a draw
    if(gs == DRAW_AGREEMENT || gs == STALEMATE)
    {
        m_winLossDrawPopupMessage = (gs == DRAW_AGREEMENT) ?
            "draw by agreement" : "draw by stalemate";
        return;
    }

    if(app.isPairedWithOpponent())
    {
        m_winLossDrawPopupMessage = "you ";
        switch(gs)
        {
        case CHECKMATE:
        {
            m_winLossDrawPopupMessage.append(whosTurnIsIt == board.getSideUserIsPlayingAs() ? "lost " : "won ");
            m_winLossDrawPopupMessage.append("by checkmate");
            break;
        }
        case OPPONENT_RESIGNED:
        {
            m_winLossDrawPopupMessage.append("won (opponent resigned)");
            break;
        }
        case YOU_RESIGNED: m_winLossDrawPopupMessage.append("lost (you resgined)");
        }
    }
    else if(gs == GAME_ABANDONMENT) m_winLossDrawPopupMessage = "opponent abandoned their game or lost connection";
    else//if not a draw (and if offline but not game abandonment) then the type is win or loss by checkmate
    {
        m_winLossDrawPopupMessage = whosTurnIsIt == Side::BLACK ? "black " : "white ";
        m_winLossDrawPopupMessage.append("lost by checkmate");
    }
}

bool ChessDrawer::isScreenPositionOnBoard(Vec2i const screenPos) const
{
    //ImGui::IsWindowHovered() should handle this. There seems to be a bug in their code.
    //It does not work in the case where I click on the options menu bar 
    //drop down, and then click again on it while the drop down is open.
    bool const belowMenuBar{screenPos.y > static_cast<int>(m_menuBarSize.y)};

    return !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && belowMenuBar;
}

void ChessDrawer::loadPieceTexturesFromDisk(std::array<std::string, NUMOF_PIECE_TEXTURES> const& filePaths)
{   
    SDL_Renderer* const renderer = ChessApp::getApp().getCurrentRenderer();
    
    for(int i = 0; i < NUMOF_PIECE_TEXTURES; ++i)
    {
        if(!(m_pieceTextures[i] = IMG_LoadTexture(renderer, filePaths[i].c_str())))
            throw std::runtime_error(IMG_GetError());
    }

    //get the width and height of each texture
    for(int i = 0; i < NUMOF_PIECE_TEXTURES; ++i)
    {
        SDL_QueryTexture(m_pieceTextures[i], nullptr, 
            nullptr, &m_pieceTextureSizes[i].x, &m_pieceTextureSizes[i].y);
    }
}

void ChessDrawer::serializeSquareColorData()
{
    std::ofstream ofs("squareColorData.txt");//dont append, but replace

    ofs << "#this is just a file for saving the color of the squares\n"
           "#'L' or 'D' means the next 3 numbers are space seperated 0-255 R G B colors\n"
           "#the L means the light square colors and the D means the dark square colors\n\n";

    //save the light square color
    ofs << "L ";
    for(auto const& i : m_lightSquareColor)
        ofs << i << ' ';

    ofs << '\n';

    //save the dark square color
    ofs << "D ";
    for(auto const& i : m_darkSquareColor) 
        ofs << i << ' ';
}

//looks for a file in the same path as the .exe called squareColorData.txt.
//if it cant find it, then it will throw an exception.
void ChessDrawer::deserializeAndLoadSquareColorData()
{
    std::ifstream ifs(SQUARE_COLOR_DATA_FILENAME);
    if(!ifs)
    {
        FileErrorLogger::get().logErrors("Could not find " SQUARE_COLOR_DATA_FILENAME 
            ". Using default square colors instead");
        return;
    }

    //a comment will start with a '#' character
    //light squares will start with L, then a space, then the 3
    //R G B numbers as 0-255 integers with spaces in between.
    //dark squares are the same but start with D.
    for(std::string currentLine; std::getline(ifs, currentLine);)
    {
        if(currentLine[0] == '#') continue;       

        //else the line represents light or dark square colors...

        std::stringstream ss(currentLine);
        char LDchar{};
        ss >> LDchar;//store the first 'L' or 'D'

        for(int i = 0; i < 4; ++i)
        {
            uint32_t RGB{};
            ss >> RGB;
            if(LDchar == 'L') m_lightSquareColor[i] = RGB;//if the current line is light square colors
            else m_darkSquareColor[i] = RGB;//else if the current line is dark square colors
        }
    }
}
