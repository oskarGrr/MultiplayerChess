#include "ChessApplication.h"
#include "PieceTypes.h"
#include <memory>//std::make_unique && std::unique_ptr

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "SDL.h"

ChessApp ChessApp::s_theApplication{};

ChessApp::ChessApp()
    : m_chessBoardWidth(896u), m_chessBoardHeight(896u), 
      m_menuBarHeight(0.0f), m_squareSize(m_chessBoardWidth / 8),
      m_lightSquareColor{214.0f/255, 235.0f/255, 225.0f/255, 255.0f/255}, 
      m_darkSquareColor{43.0f/255, 86.0f/255, 65.0f/255, 255.0f/255},
      m_wnd(m_chessBoardWidth, m_chessBoardHeight, "Chess", SDL_INIT_VIDEO, 0u), m_board{}, 
      m_circleTexture(nullptr), m_redCircleTexture(nullptr)
{
    initCircleTexture(m_squareSize / 6, 0x6F, 0x6F, 0x6F, 0x9F, &m_circleTexture);
    initCircleTexture(m_squareSize / 6, 0xDE, 0x31, 0x63, 0x7F, &m_redCircleTexture);
}

ChessApp::~ChessApp()
{
    SDL_DestroyTexture(m_circleTexture);
    SDL_DestroyTexture(m_redCircleTexture);
}

//return true if we should close the app
bool ChessApp::processEvents()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {   
        //allow imgui to proccess its own events and update the IO state
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch(event.type)
        {
        case SDL_QUIT: return true;
        case SDL_MOUSEBUTTONDOWN: s_theApplication.m_board.piecePickUpRoutine(event); break;
        case SDL_MOUSEBUTTONUP:   s_theApplication.m_board.piecePutDownRoutine(event);
        }
    }

    return false;
}

//called once per frame in ChessApp::run() to render everything (even the imgui stuff)
void ChessApp::renderAllTheThings()
{
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    //shorter names
    auto& app = s_theApplication;
    auto& board = app.m_board;

    if(app.m_wnd.m_ColorEditorWindowIsOpen) [[unlikely]]
    {
        ImGui::Begin("change square colors", 
            &app.m_wnd.m_ColorEditorWindowIsOpen);

        ImGui::TextUnformatted("light squares");
        ImGui::ColorPicker3("light squares", &app.m_lightSquareColor[0]);
        ImGui::Separator();
        ImGui::TextUnformatted("dark squares");
        ImGui::ColorPicker3("dark squares", &app.m_darkSquareColor[0]);

        ImGui::End();
    }

    if(ImGui::BeginMainMenuBar()) [[unlikely]]
    {
        if(ImGui::BeginMenu("options"))
        {   
            ImGui::MenuItem("change colors", nullptr, 
                &app.m_wnd.m_ColorEditorWindowIsOpen);
               
            ImGui::MenuItem("show ImGui demo", nullptr, 
                &app.m_wnd.m_demoWindowIsOpen);

            ImGui::EndMenu();
        }

        if(ImGui::Button("flip board", {70, 20}))
            s_theApplication.flipBoard();

        static bool needMenuBarSize = true;
        if(needMenuBarSize) [[unlikely]]
        {   
            app.m_menuBarHeight = ImGui::GetWindowSize().y;

            SDL_SetWindowSize(app.m_wnd.m_window, app.m_wnd.m_width, 
                app.m_wnd.m_height + app.m_menuBarHeight);

            for(auto const &p : app.m_board.m_pieces)
            {
                if(!p) continue;
                auto sp = chess2ScreenPos(p->getChessPosition());
                p->setScreenPos(sp);
            }

            needMenuBarSize = false;
        }

        ImGui::EndMainMenuBar();
    }

    if(app.m_wnd.m_demoWindowIsOpen)
        ImGui::ShowDemoWindow();

    ImGui::Render();
    SDL_RenderClear(app.m_wnd.m_renderer);
    drawSquares();
    drawPieces();
    drawIndicatorCircles();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(app.m_wnd.m_renderer);
}

void ChessApp::run()
{
    bool shouldQuit = false;
    while(!shouldQuit)
    {   
        shouldQuit = processEvents();       
        renderAllTheThings();
        SDL_Delay(10);
    }
}

//generates a circle texture 1 time at startup for use in the program
void ChessApp::initCircleTexture
(int radius, Uint8 RR, Uint8 GG, Uint8 BB, Uint8 AA, SDL_Texture** toInit)
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 rMask = 0xFF000000;
    Uint32 gMask = 0x00FF0000;
    Uint32 bMask = 0x0000FF00;
    Uint32 aMask = 0x000000FF;
    Uint32 circleColor = (RR << 24) + (GG << 16) + (BB << 8) + AA;
#else
    Uint32 rMask = 0x000000FF;
    Uint32 gMask = 0x0000FF00;
    Uint32 bMask = 0x00FF0000;
    Uint32 aMask = 0xFF000000;
    Uint32 circleColor = (AA << 24) + (BB << 16) + (GG << 8) + RR;
#endif
    int const diameter = radius * 2;
    SDL_Rect const boundingBox{-radius, -radius, diameter, diameter};
    Uint32 const pixelCount = diameter * diameter;
    auto pixels = std::make_unique<Uint32[]>(pixelCount);

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
         diameter * sizeof(Uint32), 
         rMask, gMask, bMask, aMask
    );
   
    if(!surface)
    {
        std::cerr << SDL_GetError();
        std::terminate();
    }

    *toInit = SDL_CreateTextureFromSurface
    (
        s_theApplication.getCurrentRenderer(), 
        surface
    );

    SDL_FreeSurface(surface);
}

//tells if a chess position is on the board or not
bool ChessApp::inRange(Vec2i const chessPos)
{
    return (chessPos.x <= 7 && chessPos.x >= 0 && chessPos.y <= 7 && chessPos.y >= 0);
}

void ChessApp::promotionRoutine(Vec2i promotionSquare, Side side)
{
    Vec2i const promotionSquareScreenPos = chess2ScreenPos(promotionSquare);
    SDL_Renderer* renderer = ChessApp::getCurrentRenderer();
    bool const wasPawnWhite = (side == Side::WHITE);

    SDL_Rect popupRect =
    {
        promotionSquareScreenPos.x - s_theApplication.m_squareSize * 0.5f,
        promotionSquareScreenPos.y - s_theApplication.m_squareSize * 0.5f,
        s_theApplication.m_squareSize,
        s_theApplication.m_squareSize * 4
    };

    //true if the promotion popup should be on the bottom of the screen (the board has been flipped)
    bool const hasBoardBeenFlipped = side != s_theApplication.m_board.getViewingPerspective();

    if(hasBoardBeenFlipped)
    {
        //move up three and a half squares
        popupRect.y = promotionSquareScreenPos.y - s_theApplication.m_squareSize * 3.5;
    }
    else
    {
        popupRect.y = promotionSquareScreenPos.y - s_theApplication.m_squareSize * 0.5f;
    }

    using enum textureIndices;
    auto const& pieceTextures = Piece::getPieceTextures();
    std::array<SDL_Texture*, 4> textures
    {
        pieceTextures[static_cast<Uint32>(wasPawnWhite ? WQUEEN  : BQUEEN)],
        pieceTextures[static_cast<Uint32>(wasPawnWhite ? WROOK   : BROOK)],
        pieceTextures[static_cast<Uint32>(wasPawnWhite ? WKNIGHT : BKNIGHT)],
        pieceTextures[static_cast<Uint32>(wasPawnWhite ? WBISHOP : BBISHOP)]
    };

    std::array<SDL_Rect, 4> sources{};//how big are the textures before any scaling
    std::array<SDL_Rect, 4> destinations{};//where will the textures be drawn and how big after scaling

    constexpr float textureScale = Piece::getPieceTextureScale();

    //fill in the source and destination rectancles for every texture
    for(int i = 0, yOffset = 0; i < 4; ++i, yOffset += s_theApplication.m_squareSize)
    {
        SDL_QueryTexture(textures[i], nullptr, nullptr, &sources[i].w, &sources[i].h);
        destinations[i] = 
        {
            promotionSquareScreenPos.x - static_cast<int>(sources[i].w * textureScale * 0.5f),
            promotionSquareScreenPos.y - static_cast<int>(sources[i].h * textureScale * 0.5f),
            static_cast<int>(sources[i].w * textureScale),
            static_cast<int>(sources[i].h * textureScale)
        };

        //slide the pieces down/up
        if(side != s_theApplication.m_board.getViewingPerspective())//up
        {
            destinations[i].y -= yOffset;
        }
        else//down
        {
            destinations[i].y += yOffset;
        }
    }

    bool hasSelectionBeenMade = false;
    while(!hasSelectionBeenMade)
    {
        SDL_RenderClear(renderer);
        drawSquares();
        drawPieces();

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &popupRect);

        for(int i = 0; i < 4; ++i)
            SDL_RenderCopy(renderer, textures[i], &sources[i], &destinations[i]);

        SDL_Event e;
        while(SDL_PollEvent(&e))
        {
            if(e.type == SDL_QUIT)
                exit(0);
            
            if(e.type == SDL_MOUSEBUTTONDOWN)
            {
                if(ChessApp::isMouseOver(popupRect))
                {   
                    int y = 0;
                    SDL_GetMouseState(nullptr, &y);

                    //to avoid a memory leak by overiting the Piece* at promotionSquareChessPos
                    //(the pawn that promoted is still there) capture the pawn there before placing a new piece down   
                    s_theApplication.m_board.capturePiece(promotionSquare);

                    //the promotion popup will look like this cascading down/up from the promotion square
                    // down 0  |Q|      up 3 |B|  
                    //      1  |R|         2 |N|
                    //      2  |N|         1 |R|
                    //      3  |B|         0 |Q|

                    int const whichPiece = hasBoardBeenFlipped ? 
                        7 - y / s_theApplication.m_squareSize : y / s_theApplication.m_squareSize;

                    switch(whichPiece)
                    {
                    case 0:
                        s_theApplication.m_board.makeNewPieceAt<Queen>(promotionSquare, side);
                        hasSelectionBeenMade = true;
                        break;
                    case 1:
                        s_theApplication.m_board.makeNewPieceAt<Rook>(promotionSquare, side);
                        hasSelectionBeenMade = true;
                        break;
                    case 2:
                        s_theApplication.m_board.makeNewPieceAt<Knight>(promotionSquare, side);
                        hasSelectionBeenMade = true;
                        break;
                    case 3:
                        s_theApplication.m_board.makeNewPieceAt<Bishop>(promotionSquare, side);
                        hasSelectionBeenMade = true;
                        break;
                    }
                }
            }
        }
        SDL_RenderPresent(renderer);
    }
}

//tells wether mouse position is over a given rectangle
bool ChessApp::isMouseOver(SDL_Rect const& r)
{
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    return(x >= r.x && x <= r.x+r.w && y >= r.y && y <= r.h + r.y);
}

void ChessApp::flipBoard()
{
    m_board.m_viewingPerspective = m_board.m_viewingPerspective == Side::WHITE ? 
        Side::BLACK : Side::WHITE;

    for(auto const& p : m_board.m_pieces)
    { 
        if(!p) continue;
        auto sp = chess2ScreenPos(p->getChessPosition());
        p->setScreenPos(sp);
    }
}

//take a chess position and flip it to the correct screen position
//based on which perspective the player is viewing the board from
Vec2i ChessApp::chess2ScreenPos(Vec2i const pos)
{
    Vec2i ret{pos};

    if(s_theApplication.m_board.getViewingPerspective() == Side::WHITE)
    {
        ret.y = 7 - ret.y;
    }
    else//if viewing the board from blacks perspective
    {
        ret.x = 7 - ret.x;
    }

    //scale up to board size
    ret.x *= s_theApplication.m_squareSize;
    ret.y *= s_theApplication.m_squareSize;

    //move from top left to middle of square
    ret.y += s_theApplication.m_squareSize * 0.5f;
    ret.x += s_theApplication.m_squareSize * 0.5f;

    //move down to account for the menu bar (will only work after the first frame)
    ret.y += s_theApplication.m_menuBarHeight;

    return ret;
}

//wont check if pos is on the chess board callee must do that
Vec2i ChessApp::screen2ChessPos(Vec2i const pos)
{
    Vec2i ret
    {
        pos.x / s_theApplication.m_squareSize,
        pos.y / s_theApplication.m_squareSize
    };

    if(s_theApplication.m_board.getViewingPerspective() == Side::WHITE)
    {
        ret.y = 7 - ret.y;
    }
    else
    {
        ret.x = 7 - ret.x;
    }

    return ret;
}

bool ChessApp::isPositionOnBoard(Vec2i const pos)
{
    auto& app = s_theApplication;//shorter name
    return pos.x > 0 && 
           pos.x < app.m_chessBoardWidth && 
           pos.y > app.m_menuBarHeight &&
           pos.y < app.m_wnd.m_height;
}

void ChessApp::drawIndicatorCircles()
{
    auto const pom = Piece::getPieceOnMouse();
    if(!pom) return;

    SDL_Renderer *const renderer  = ChessApp::getCurrentRenderer();
    SDL_Texture  *const circle    = ChessApp::getCircleTexture();
    SDL_Texture  *const redCircle = ChessApp::getRedCircleTexture();

    SDL_Rect circleSource{};
    SDL_QueryTexture(circle, nullptr, nullptr, &circleSource.w, &circleSource.h);

    for(auto const& [move, moveType] : pom->getLegalMoves())
    {
        Vec2i const circlePos{ChessApp::chess2ScreenPos(move)};
        SDL_Rect const circleDest
        {
            circlePos.x - circleSource.w * 0.5f,
            circlePos.y - circleSource.w * 0.5f,
            circleSource.w,
            circleSource.h
        };

        //if there is an enemy piece or enPassant square draw red circle instead
        auto const piece = s_theApplication.m_board.getPieceAt(move);
        if(piece && piece->getSide() != s_theApplication.m_board.getWhosTurnItIs() || 
           move == s_theApplication.m_board.getEnPassantIndex())
        {
            SDL_RenderCopy(renderer, redCircle, &circleSource, &circleDest);
            continue;
        }

        SDL_RenderCopy(renderer, circle, &circleSource, &circleDest);
    }
}

void ChessApp::drawSquares()
{
    auto& app = s_theApplication;//shorter name
    SDL_Rect square{0, app.m_menuBarHeight, app.m_squareSize,  app.m_squareSize};
    SDL_Renderer *const renderer = ChessApp::getCurrentRenderer();

    for(int i = 0; i < 8; ++i, square.x += app.m_squareSize)
    {
        for(int j = 0; j < 8; ++j, square.y += app.m_squareSize)
        {
            if( !( i + j & 1 ) )//if light square
            {
                //scale up the colors from 0-1 to 0-255 and draw
                SDL_SetRenderDrawColor
                (
                    renderer, 
                    app.m_lightSquareColor[0] * 255.0f, 
                    app.m_lightSquareColor[1] * 255.0f, 
                    app.m_lightSquareColor[2] * 255.0f, 
                    app.m_lightSquareColor[3] * 255.0f
                );
            }
            else//if dark square
            {
                SDL_SetRenderDrawColor
                (
                    renderer,
                    app.m_darkSquareColor[0] * 255.0f,
                    app.m_darkSquareColor[1] * 255.0f,
                    app.m_darkSquareColor[2] * 255.0f,
                    app.m_darkSquareColor[3] * 255.0f
                );
            }

            SDL_RenderFillRect(renderer, &square);
        }

        square.y = app.m_menuBarHeight;
    }
}

void ChessApp::drawPieces()
{
    auto const pom = Piece::getPieceOnMouse();

    //draw all the pieces and defer the draw() call for the piece 
    //on the mouse until the end of the function
    for(auto const& piece : s_theApplication.m_board.m_pieces)
        if(piece && piece != pom) piece->draw();

    if(pom) pom->drawPieceOnMouse();
}