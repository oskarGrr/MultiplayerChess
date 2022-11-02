#include "ChessApplication.h"
#include "PieceTypes.h"
#include <memory>//std::make_unique/make_shared and std::unique_ptr/shared_ptr
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "SDL.h"
#include "SDL_image.h"

ChessApp ChessApp::s_theApplication{};

ChessApp::ChessApp() :
      m_chessBoardWidth(896u), m_chessBoardHeight(896u), 
      m_squareSize(m_chessBoardWidth / 8), m_menuBarHeight(0.0f),
      m_wnd(m_chessBoardWidth, m_chessBoardHeight, "Chess", SDL_INIT_VIDEO | SDL_INIT_AUDIO, 0u), m_board{}, 
      m_pieceMoveSound("sounds/woodChessMove.wav"), m_pieceCastleSound("sounds/woodChessCastle.wav"),
      m_lightSquareColor{214, 235, 225, 255}, m_darkSquareColor{43, 86, 65, 255},
      m_circleTexture(nullptr), m_redCircleTexture(nullptr),
      m_pieceTextureScale(1.0f), m_pieceTextures{}
{
    initCircleTexture(m_squareSize / 6, 0x6F, 0x6F, 0x6F, 0x9F, &m_circleTexture);
    initCircleTexture(m_squareSize / 6, 0xDE, 0x31, 0x63, 0x7F, &m_redCircleTexture);    
    loadPieceTexturesFromDisk();
}

ChessApp::~ChessApp()
{
    SDL_DestroyTexture(m_circleTexture);
    SDL_DestroyTexture(m_redCircleTexture);
    for(auto* t : m_pieceTextures) SDL_DestroyTexture(t);
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
        case SDL_MOUSEBUTTONDOWN: m_board.piecePickUpRoutine(event); break;
        case SDL_MOUSEBUTTONUP:   m_board.piecePutDownRoutine(event);
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

    if(m_wnd.m_ColorEditorWindowIsOpen) [[unlikely]]
    {
        ImGui::Begin("change square colors", 
            &m_wnd.m_ColorEditorWindowIsOpen);

        ImGui::TextUnformatted("light squares");

        ImVec4 f_lightSquares{};//a float (0-1) version of the light squares
        ImVec4 f_darkSquares{};//a float (0-1) version of the dark squares
        for(std::size_t i = 0; i < 4; ++i)
        {
            f_lightSquares[i] = m_lightSquareColor[i] / 255.0f;
            f_darkSquares[i] = m_darkSquareColor[i] / 255.0f;
        }

        ImGui::ColorPicker3("light squares", &f_lightSquares[0]);
        ImGui::Separator();
        ImGui::TextUnformatted("dark squares");
        ImGui::ColorPicker3("dark squares", &f_darkSquares[0]);

        for(std::size_t i = 0; i < 4; ++i)
        {
            m_lightSquareColor[i] = static_cast<Uint8>(f_lightSquares[i] * 255);
            m_darkSquareColor[i] = static_cast<Uint8>(f_darkSquares[i] * 255);
        }

        ImGui::End();
    }

    if(ImGui::BeginMainMenuBar()) [[unlikely]]
    {
        if(ImGui::BeginMenu("options"))
        {   
            ImGui::MenuItem("change colors", nullptr, 
                &m_wnd.m_ColorEditorWindowIsOpen);
               
            ImGui::MenuItem("show ImGui demo", nullptr, 
                &m_wnd.m_demoWindowIsOpen);

            ImGui::EndMenu();
        }

        if(ImGui::Button("flip board", {70, 20}))
            flipBoard();

        static bool needMenuBarSize = true;
        if(needMenuBarSize) [[unlikely]]
        {   
            m_menuBarHeight = ImGui::GetWindowSize().y;

            SDL_SetWindowSize(m_wnd.m_window, m_wnd.m_width,
                static_cast<int>(m_wnd.m_height + m_menuBarHeight));

            needMenuBarSize = false;
        }

        ImGui::EndMainMenuBar();
    }

    if(m_wnd.m_demoWindowIsOpen)
        ImGui::ShowDemoWindow();

    ImGui::Render();
    SDL_RenderClear(m_wnd.m_renderer);
    drawSquares();
    drawIndicatorCircles();
    drawPieces();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(m_wnd.m_renderer);
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

//generates a circle texture at startup to use later
void ChessApp::initCircleTexture
(int radius, Uint8 RR, Uint8 GG, Uint8 BB, Uint8 AA, SDL_Texture** toInit)
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

    *toInit = SDL_CreateTextureFromSurface(getCurrentRenderer(), surface);
    SDL_FreeSurface(surface);
}

void ChessApp::playChessMoveSound()
{
    s_theApplication.m_pieceMoveSound.playFullSound();
}

void ChessApp::playChessCastleSound()
{
    s_theApplication.m_pieceCastleSound.playFullSound();
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
        static_cast<int>(promotionSquareScreenPos.x - m_squareSize * 0.5f),
        static_cast<int>(promotionSquareScreenPos.y - m_squareSize * 0.5f),
        static_cast<int>(m_squareSize),
        static_cast<int>(m_squareSize * 4)
    };

    //true if the promotion popup should be on the bottom of the screen (the board has been flipped)
    bool const hasBoardBeenFlipped = side != m_board.getViewingPerspective();

    if(hasBoardBeenFlipped)
    {
        //move up three and a half squares
        popupRect.y = static_cast<int>(promotionSquareScreenPos.y - m_squareSize * 3.5);
    }
    else
    {
        popupRect.y = static_cast<int>(promotionSquareScreenPos.y - m_squareSize * 0.5f);
    }

    using enum TextureIndices;
    std::array<SDL_Texture*, 4> textures
    {
        m_pieceTextures[static_cast<Uint32>(wasPawnWhite ? WQUEEN  : BQUEEN)],
        m_pieceTextures[static_cast<Uint32>(wasPawnWhite ? WROOK   : BROOK)],
        m_pieceTextures[static_cast<Uint32>(wasPawnWhite ? WKNIGHT : BKNIGHT)],
        m_pieceTextures[static_cast<Uint32>(wasPawnWhite ? WBISHOP : BBISHOP)]
    };

    std::array<SDL_Rect, 4> sources{};//how big are the textures before any scaling
    std::array<SDL_Rect, 4> destinations{};//where will the textures be drawn and how big after scaling

    //fill in the source and destination rectancles for every texture
    for(int i = 0, yOffset = 0; i < 4; ++i, yOffset += m_squareSize)
    {
        SDL_QueryTexture(textures[i], nullptr, nullptr, &sources[i].w, &sources[i].h);
        destinations[i] = 
        {
            promotionSquareScreenPos.x - static_cast<int>(sources[i].w * m_pieceTextureScale * 0.5f),
            promotionSquareScreenPos.y - static_cast<int>(sources[i].h * m_pieceTextureScale * 0.5f),
            static_cast<int>(sources[i].w * m_pieceTextureScale),
            static_cast<int>(sources[i].h * m_pieceTextureScale)
        };

        //slide the pieces down/up
        if(side != m_board.getViewingPerspective())//up
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
                    m_board.capturePiece(promotionSquare);

                    //the promotion popup will look like this cascading down/up from the promotion square
                    // down 0  |Q|      up 3 |B|  
                    //      1  |R|         2 |N|
                    //      2  |N|         1 |R|
                    //      3  |B|         0 |Q|

                    int const whichPiece = hasBoardBeenFlipped ? 
                        7 - y / m_squareSize : y / m_squareSize;

                    switch(whichPiece)
                    {
                    case 0:
                        m_board.makeNewPieceAt<Queen>(promotionSquare, side);
                        hasSelectionBeenMade = true;
                        break;
                    case 1:
                        m_board.makeNewPieceAt<Rook>(promotionSquare, side);
                        hasSelectionBeenMade = true;
                        break;
                    case 2:
                        m_board.makeNewPieceAt<Knight>(promotionSquare, side);
                        hasSelectionBeenMade = true;
                        break;
                    case 3:
                        m_board.makeNewPieceAt<Bishop>(promotionSquare, side);
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
    ret.y += static_cast<int>(s_theApplication.m_squareSize * 0.5f);
    ret.x += static_cast<int>(s_theApplication.m_squareSize * 0.5f);

    //move down to account for the menu bar (will only work after the first frame)
    ret.y += static_cast<int>(s_theApplication.m_menuBarHeight);

    return ret;
}

//wont check if pos is on the chess board callee must do that
Vec2i ChessApp::screen2ChessPos(Vec2i const pos)
{
    Vec2i ret
    {
        static_cast<int>(pos.x / s_theApplication.m_squareSize),
        static_cast<int>(pos.y / s_theApplication.m_squareSize)
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

bool ChessApp::isPositionOnBoard(Vec2i const screenPosition)
{
    auto const& app = s_theApplication;//shorter name
    return screenPosition.x > 0 && 
           static_cast<Uint32>(screenPosition.x) < app.m_chessBoardWidth &&
           screenPosition.y > app.m_menuBarHeight &&
           screenPosition.y < app.m_wnd.m_height;
}

void ChessApp::drawIndicatorCircles()
{
    auto const pom = Piece::getPieceOnMouse();
    if(!pom) return;

    SDL_Renderer *const renderer  = ChessApp::getCurrentRenderer();

    SDL_Rect circleSource{};
    SDL_QueryTexture(m_circleTexture, nullptr, nullptr, &circleSource.w, &circleSource.h);

    for(auto const& [move, moveType] : pom->getLegalMoves())
    {
        Vec2i const circlePos{ChessApp::chess2ScreenPos(move)};
        SDL_Rect const circleDest
        {
            static_cast<int>(circlePos.x - circleSource.w * 0.5f),
            static_cast<int>(circlePos.y - circleSource.w * 0.5f),
            static_cast<int>(circleSource.w),
            static_cast<int>(circleSource.h)
        };

        //if there is an enemy piece or enPassant square draw red circle instead
        auto const piece = m_board.getPieceAt(move);
        if(piece && piece->getSide() != m_board.getWhosTurnItIs() || 
           move == m_board.getEnPassantIndex())
        {
            SDL_RenderCopy(renderer, m_redCircleTexture, &circleSource, &circleDest);
            continue;
        }

        SDL_RenderCopy(renderer, m_circleTexture, &circleSource, &circleDest);
    }
}

void ChessApp::drawSquares()
{
    SDL_Rect square{0, static_cast<int>(m_menuBarHeight), 
        static_cast<int>(m_squareSize), static_cast<int>(m_squareSize)};

    SDL_Renderer *const renderer = ChessApp::getCurrentRenderer();

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

        square.y = static_cast<int>(m_menuBarHeight);
    }
}

void ChessApp::drawPieces()
{
    auto const pom = Piece::getPieceOnMouse();

    //draw all the pieces and defer the draw() call for the piece 
    //on the mouse until the end of the function
    for(auto const& piece : s_theApplication.m_board.m_pieces)
    { 
        if(piece && piece != pom)
        {
            //figure out where on the screen the piece is (the middle of the square)
            Vec2i screenPosition = chess2ScreenPos(piece->getChessPosition());
            
            //get the width and height of whichever texture the piece on the mouse is
            Vec2i textureSize = m_pieceTextureSizes[static_cast<Uint32>(piece->getWhichTexture())];
            
            //from the screen position figure out the destination rectangle
            SDL_Rect dest
            {
                .x = screenPosition.x - textureSize.x / 2, 
                .y = screenPosition.y - textureSize.y / 2, 
                .w = textureSize.x, 
                .h = textureSize.y
            };
            
            SDL_RenderCopy
            (
                ChessApp::getCurrentRenderer(), 
                m_pieceTextures[static_cast<Uint32>(piece->getWhichTexture())],
                nullptr, &dest
            );
        }
    }

    if(pom)
    {
        Vec2i screenPosition = chess2ScreenPos(pom->getChessPosition());
        Vec2i textureSize = m_pieceTextureSizes[static_cast<Uint32>(pom->getWhichTexture())];

        //create a destination where the mouse cursor is
        int mouseX = 0, mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        SDL_Rect mouseDestination
        {
            .x = mouseX - textureSize.x / 2,
            .y = mouseY - textureSize.y / 2,
            .w = textureSize.x, 
            .h = textureSize.y
        };

        SDL_RenderCopy
        (
            ChessApp::getCurrentRenderer(), 
            m_pieceTextures[static_cast<Uint32>(pom->getWhichTexture())],
            nullptr, &mouseDestination
        );
    }
}

void ChessApp::loadPieceTexturesFromDisk()
{
    using enum TextureIndices;
    SDL_Renderer* renderer = ChessApp::getCurrentRenderer();
    if(!(m_pieceTextures[static_cast<Uint32>(WPAWN)]   = IMG_LoadTexture(renderer, "textures/wPawn.png"))) 
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(WKNIGHT)] = IMG_LoadTexture(renderer, "textures/wKnight.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(WROOK)]   = IMG_LoadTexture(renderer, "textures/wRook.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(WBISHOP)] = IMG_LoadTexture(renderer, "textures/wBishop.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(WQUEEN)]  = IMG_LoadTexture(renderer, "textures/wQueen.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(WKING)]   = IMG_LoadTexture(renderer, "textures/wKing.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(BPAWN)]   = IMG_LoadTexture(renderer, "textures/bPawn.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(BKNIGHT)] = IMG_LoadTexture(renderer, "textures/bKnight.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(BROOK)]   = IMG_LoadTexture(renderer, "textures/bRook.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(BBISHOP)] = IMG_LoadTexture(renderer, "textures/bBishop.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(BQUEEN)]  = IMG_LoadTexture(renderer, "textures/bQueen.png")))
        throw IMG_GetError();
    if(!(m_pieceTextures[static_cast<Uint32>(BKING)]   = IMG_LoadTexture(renderer, "textures/bKing.png")))
        throw IMG_GetError();

    //get the width and height of each texture
    for(int i = 0; i < NUM_OF_PIECE_TEXTURES; ++i)
    {
        SDL_QueryTexture(m_pieceTextures[i], nullptr, nullptr, 
            &m_pieceTextureSizes[i].x, &m_pieceTextureSizes[i].y);

        //scale the texture up or down to the correct size
        m_pieceTextureSizes[i] *= m_pieceTextureScale;
    }
}
