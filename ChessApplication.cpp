#include "ChessApplication.h"
#include "PieceTypes.h"
#include <memory>//std::make_unique/make_shared and std::unique_ptr/shared_ptr
#include <fstream>//std::ofstream
#include <chrono>
#include <ctime>
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "SDL.h"
#include "SDL_image.h"
#include "ChessNetworking.h"

ChessApp ChessApp::s_theApplication{};

#define NUM_OF_PIECE_TEXTURES 12 //6 types of pieces * 2 for both sides
#define SQUARE_COLOR_DATA_FILENAME "squareColorData.txt"

ChessApp::ChessApp() try :
      m_gameState{GameState::INVALID}, m_chessBoardWidth(896u), m_chessBoardHeight(896u),
      m_squareSize(m_chessBoardWidth / 8), m_menuBarHeight(0.0f),
      m_wnd(m_chessBoardWidth, m_chessBoardHeight, "Chess", SDL_INIT_VIDEO | SDL_INIT_AUDIO, 0u), m_board{},
      m_pieceMoveSound("sounds/woodChessMove.wav"), m_pieceCastleSound("sounds/woodChessCastle.wav"),
      m_pieceCaptureSound("sounds/woodCaptureMove.wav"),
      m_lightSquareColor{214, 235, 225, 255}, m_darkSquareColor{43, 86, 65, 255},//default square colors
      m_circleTexture(nullptr), m_redCircleTexture(nullptr),
      m_pieceTextureScale(1.0f), m_pieceTextures{}, m_netWork{}, m_winLossDrawPopupMessage{}
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    initCircleTexture(m_squareSize / 6, 0x6F, 0x6F, 0x6F, 0x9F, &m_circleTexture);
    initCircleTexture(m_squareSize / 6, 0xDE, 0x31, 0x63, 0x7F, &m_redCircleTexture);
    loadPieceTexturesFromDisk();
    deserializeAndLoadSquareColorData();
}
catch(std::exception& e)
{
    std::ofstream ofs("log.txt", std::ios_base::app);
    ofs << "exception (of type" << e.what() << ") thrown durring construction of ChessApp::s_theApplication\n"
        << "..or any of ChessApp::s_theApplication's members or base ctors "
        << "at " << ChessApp::getCurrentDateAndTime() << "\n\n";
}
catch(...)
{
    std::ofstream ofs("log.txt", std::ios_base::app);
    ofs << "Unandled exception of unknown type"
        << "(not deriving from std::exception or a simple const char*)\n "
        << "while constructing ChessApp::s_theApplication"
        << "at " << ChessApp::getCurrentDateAndTime() << "\n\n";
}

ChessApp::~ChessApp()
{
    //free textures
    SDL_DestroyTexture(m_circleTexture);
    SDL_DestroyTexture(m_redCircleTexture);
    for(auto t : m_pieceTextures) SDL_DestroyTexture(t);

    //save the square colors
    serializeSquareColorData();
}

void ChessApp::run()
{
    setGameState(GameState::GAME_IN_PROGRESS);
    bool shouldQuit = false;
    while(!shouldQuit)
    {
        processIncomingMessages();
        shouldQuit = processEvents();
        checkForDisconnect();
        renderAllTheThings();
        SDL_Delay(10);
    }
}

//return true if we should close the app
bool ChessApp::processEvents()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        //allow imgui to process its own events and update the IO state
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
   
    if(!surface) throw std::exception(SDL_GetError());

    *toInit = SDL_CreateTextureFromSurface(getCurrentRenderer(), surface);
    SDL_FreeSurface(surface);
}

//looks for a file in the same path as the .exe called squareColorData.txt.
//if it cant find it, then it will throw an exception.
void ChessApp::deserializeAndLoadSquareColorData()
{
    std::ifstream ifs(SQUARE_COLOR_DATA_FILENAME);

    //if squareColorData.txt cant be found
    if(!ifs)
    {
        //ChessApp ctor already initialized the square colors to the default colors
        std::ofstream ofs("log.txt");
        ofs << "Could not find squareColorData.txt. Using default square colors instead.\n";
        return;
    }

    //else load the colors...

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
            Uint32 RGB{};
            ss >> RGB;
            if(LDchar == 'L') m_lightSquareColor[i] = RGB;//if the current line is light square colors
            else m_darkSquareColor[i] = RGB;//else if the current line is dark square colors
        }
    }
}

void ChessApp::serializeSquareColorData()
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

void ChessApp::drawColorEditorWindow()
{
    ImGui::Begin("change square colors", &m_wnd.m_ColorEditorWindowIsOpen,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    
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
    
    ImGui::End();
}

//called after process events in the main loop because the game state needs to be updated
//to checkmate or stalemate first if there was one
void ChessApp::checkForDisconnect()
{
    if(m_netWork.wasConnectionClosedOrLost())
    {
        //if the game was still in progress when we lost connection
        if(getGameState() == GameState::GAME_IN_PROGRESS)
            setGameState(GameState::GAME_ABANDONMENT);

        openWinLossDrawPopup();
        m_netWork.resetWasConnectionLostBool();
    }
}

//returns true if a successful connection was made
void ChessApp::drawConnectionWindow()
{
    ImGui::Begin("connect to another player", &m_wnd.m_connectWindowIsOpen, 
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    
    std::string targetIP;
    targetIP.resize(46);

    ImGui::TextUnformatted("enter an IPv4 address and then press enter");
    if(ImGui::InputText("IP address", targetIP.data(), targetIP.size(),
        ImGuiInputTextFlags_EnterReturnsTrue))
    {
        try
        {
            m_netWork.connect2Server(targetIP);
        }
        catch(networkException& ne)
        {
            (void)ne;
            //todo.. error handling lol
        }
    }
    
    ImGui::Separator();    
    ImGui::TextUnformatted("or wait a client to connect\n."
        "..this will block for 10 seconds before timing out");

    if(ImGui::Button("wait for client to connect..."))
    { 
        try
        { 
             m_netWork.waitForClient2Connect(10, 0);
        }
        catch(networkException& ne)
        {
            (void)ne;
            //todo.. error handling lol
        }
    }

    ImGui::End();
}

void ChessApp::drawResetButtonErrorPopup()
{
    ImGui::OpenPopup("error");

    //always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::BeginPopup("error", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::TextUnformatted("can't reset board when connected with another player!");
        if(ImGui::Button("okay"))
        {
            ImGui::CloseCurrentPopup();
            m_wnd.m_resetBoardPopupIsOpen = false;
        }
        ImGui::EndPopup();
    }
}

void ChessApp::drawNewConnectionPopup()
{
    ImGui::OpenPopup("successfully connected");

    //always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::BeginPopup("successfully connected"))
    {
        std::string whichPieces("you are playing with the ");
        whichPieces.append(m_board.getSideUserIsPlayingAs() ==
            Side::WHITE ? "white " : "black ");
        whichPieces.append("pieces");

        ImGui::TextUnformatted("successfully connected to");
        ImGui::SameLine();
        ImGui::TextUnformatted(m_netWork.getIpv4OfPeer().data());
        ImGui::TextUnformatted(whichPieces.data());
        if(ImGui::Button("lets play!"))
        {
            ImGui::CloseCurrentPopup();
            m_wnd.m_newConnectionPopupIsOpen = false;
        }
        ImGui::EndPopup();
    }
}

void ChessApp::drawMenuBar()
{
    ImGui::StyleColorsLight();//light menu bar
    if(ImGui::BeginMainMenuBar()) [[unlikely]]
    {
        if(ImGui::BeginMenu("options"))
        {   
            ImGui::MenuItem("change colors", nullptr, &m_wnd.m_ColorEditorWindowIsOpen);
            ImGui::MenuItem("connect to another player", nullptr, &m_wnd.m_connectWindowIsOpen);
            ImGui::MenuItem("show ImGui demo", nullptr, &m_wnd.m_demoWindowIsOpen);
            ImGui::EndMenu();
        }
    
        if(ImGui::Button("flip board", {70, 20}))
            m_board.flipBoardViewingPerspective();
    
        if(ImGui::Button("reset board", {82, 20}))
        {
            if(!m_netWork.isUserConnected()) m_board.resetBoard();
            else m_wnd.m_resetBoardPopupIsOpen = true;
        }

        if(m_netWork.isUserConnected())
        {
            if(ImGui::Button("resign", {48, 20}))
            {
                send1ByteMessage(ChessConnection::NetMessageType::RESIGN);
                setGameState(GameState::YOU_RESIGNED);
                openWinLossDrawPopup();
            }

            std::string connectionInfo{};
            connectionInfo.append("(connected to ");
            connectionInfo.append(m_netWork.getIpv4OfPeer().data());
            connectionInfo.append(" you are playing with the");
            connectionInfo.append(m_board.getSideUserIsPlayingAs() == Side::WHITE ?
                " white pieces)" : " black pieces)");

            //std::cout << connectionInfo << std::endl;

            ImGui::TextUnformatted(connectionInfo.data());
        }

        ImGui::TextUnformatted(m_board.getWhosTurnItIs() == Side::WHITE ?
            "it's white's turn" : "it's black's turn");

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
    ImGui::StyleColorsDark();//light menu bar
}

//called inside of openWinLossDrawPopup() 
//in order to update m_winLossDrawPopupMessage before drawing the win loss draw popup
void ChessApp::updateWinLossDrawMessage()
{
    using enum GameState;
    auto gs = getGameState();
    assert(gs != GAME_IN_PROGRESS);
    auto whosTurnIsIt = m_board.getWhosTurnItIs();

    if(!(gs == DRAW_AGREEMENT || gs == STALEMATE))
    {
        //if game state is game abandonment, then the user is already disconnected at this point
        if(isUserConnected())
        {
            m_winLossDrawPopupMessage = "you ";
            switch(gs)
            {
            case CHECKMATE:
            {
                m_winLossDrawPopupMessage.append(whosTurnIsIt == m_board.getSideUserIsPlayingAs() ? "lost " : "won ");
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
        else if(gs == GAME_ABANDONMENT) m_winLossDrawPopupMessage = "connection was lost or opponent closed their game";
        else//if not a draw (and if offline but not game abandonment) then the type is win or loss by checkmate
        {
            m_winLossDrawPopupMessage = whosTurnIsIt == Side::BLACK ? "black " : "white ";
            m_winLossDrawPopupMessage.append("lost by checkmate");
        }
    }
    else//if game state is a draw
    {
        m_winLossDrawPopupMessage = gs == DRAW_AGREEMENT ? 
            "draw by agreement" : "draw by stalemate";//else game was a draw
    }
}

void ChessApp::drawWinLossDrawPopup()
{
    assert(getGameState() != GameState::GAME_IN_PROGRESS);

    //always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::Begin("game over", &m_wnd.m_winLossDrawPopupIsOpen,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
    
    ImGui::TextUnformatted(m_winLossDrawPopupMessage.data());

    if(isUserConnected())
    {
        if(ImGui::Button("request rematch"))
            send1ByteMessage(ChessConnection::NetMessageType::REMATCH_REQUEST);

        if(ImGui::Button("disconnect"))
        {
            m_netWork.disconnect();
            m_board.resetBoard();
        }
    }
    else//if playing offline
    {
        if(ImGui::Button("okay"))
            m_board.resetBoard();
    }

    ImGui::End();
}

//pt will be defaulted to PromoType::INVALID to indicate no promotion is happening. Otherwise
//it will hold a value that indicates that a promotion took place and which piece to promote to.
void ChessApp::sendMove(Move const& move, PromoType pt)
{
    //pack all of the information into a string
    using enum ChessConnection::NetMessageType;
    std::string netMessage;
    netMessage.resize(ChessConnection::s_moveMessageSize);

    //the layout of the MOVE_MSG
    //|0|1|2|3|4|5|6|
    //byte 0 will be the MOVE_MSG
    //byte 1 will be the file (0-7) of the square where the piece is that will be moving
    //byte 2 will be the rank (0-7) of the square where the piece is that will be moving
    //byte 3 will be the file (0-7) of the square where the piece will be moving to
    //byte 4 will be the rank (0-7) of the square where the piece will be moving to
    //byte 5 will be the PromoType (enum defined in board.h) of the promotion if there is one
    //byte 6 will be the MoveInfo (enum defined in board.h) of the move that is happening
    netMessage[0] = static_cast<char>(MOVE);
    netMessage[1] = move.m_source.x;
    netMessage[2] = move.m_source.y;
    netMessage[3] = move.m_dest.x;
    netMessage[4] = move.m_dest.y;
    netMessage[5] = static_cast<char>(pt);
    netMessage[6] = static_cast<char>(move.m_moveType);

    m_netWork.sendMessage(MOVE, netMessage);
}

void ChessApp::send1ByteMessage(ChessConnection::NetMessageType NMT)
{
    using enum ChessConnection::NetMessageType;
    std::string msg{};
    msg.resize(1);
    msg[0] = static_cast<char>(NMT);
    m_netWork.sendMessage(NMT, msg.data());
}

//called once per frame in ChessApp::run() to render everything 
void ChessApp::renderAllTheThings()
{
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if(m_wnd.m_connectWindowIsOpen)        [[unlikely]]
        drawConnectionWindow();

    if(m_wnd.m_ColorEditorWindowIsOpen)    [[unlikely]]
        drawColorEditorWindow();     
    
    if(m_wnd.m_demoWindowIsOpen)           [[unlikely]]
        ImGui::ShowDemoWindow();
    
    if(m_wnd.m_promotionWindowIsOpen)      [[unlikely]]
        drawPromotionPopup();
        
    if(m_wnd.m_resetBoardPopupIsOpen)      [[unlikely]]
        drawResetButtonErrorPopup();

    if(m_wnd.m_newConnectionPopupIsOpen)   [[unlikely]]
        drawNewConnectionPopup();

    if(m_wnd.m_winLossDrawPopupIsOpen)     [[unlikely]]
        drawWinLossDrawPopup();

    if(m_wnd.m_rematchRequestWindowIsOpen) [[unlikely]]
        drawRematchRequestWindow();

    drawMenuBar();

    ImGui::Render();
    SDL_RenderClear(m_wnd.m_renderer);
    drawSquares();
    drawPiecesNotOnMouse();
    if(!isPromotionWndOpen()) [[likely]]
    {
        drawMoveIndicatorCircles();
        drawPieceOnMouse();
    }
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(m_wnd.m_renderer);
}

//a pointer to this is passed to P2PChessConnection::connect2Server() and 
//P2PChessConnection::waitForCLient2Connect() and will be executed 
//if the TCP connection was successfully established
void ChessApp::onNewConnection()
{
    using CT = ChessConnection::ConnectionType;
    using NMT = ChessConnection::NetMessageType;
    assert(m_netWork.isUserConnected());
    
    Side whiteOrBlack = Side::INVALID;

    //the client will pick its side first and then send that side as a message
    //to the server so that the server can pick the other side.
    //later I will add the ability to pick which pieces the users want to play as instead
    //of it being random
    if(m_netWork.isUserServerOrClient() == CT::CLIENT)
    {
        whiteOrBlack = (std::rand() & 1) ? Side::WHITE : Side::BLACK;
        m_board.setSideUserIsPlayingAs(whiteOrBlack);
        std::string sideMsg;
        sideMsg.push_back(static_cast<char>(NMT::WHICH_SIDE));
        sideMsg.push_back(static_cast<char>(whiteOrBlack));
        m_netWork.sendMessage(NMT::WHICH_SIDE, sideMsg);
    }
    else//the user is the "server"
    {
        if(auto result = m_netWork.recieveMessageIfAvailable(2, 0))
        {
            auto& msg = result.value();
            assert(static_cast<NMT>(msg.at(0)) == NMT::WHICH_SIDE);
            whiteOrBlack = static_cast<Side>(msg.at(1)) == 
                Side::BLACK ? Side::WHITE : Side::BLACK;

            m_board.setSideUserIsPlayingAs(whiteOrBlack);
        }
    }

    m_board.resetBoard();
    m_board.setBoardViewingPerspective(whiteOrBlack);
    m_wnd.m_newConnectionPopupIsOpen = true;
    m_wnd.m_connectWindowIsOpen = false;
}

//handle the incomming move message from the opponent
void ChessApp::handleMoveMessage(std::string_view msg)
{
    assert(msg.size() == ChessConnection::s_moveMessageSize);
    MoveInfo moveType = static_cast<MoveInfo>(msg.at(6));
    Move move = {{msg.at(1), msg.at(2)}, {msg.at(3), msg.at(4)}, moveType};
    PromoType pt = static_cast<PromoType>(msg.at(5));
    m_board.movePiece(move);
    m_board.postMoveUpdate(move, pt);
}

void ChessApp::handleResignMessage()
{
    setGameState(GameState::OPPONENT_RESIGNED);
    openWinLossDrawPopup();
}

//no draw offer button yet
void ChessApp::handleDrawOfferMessage()
{
    
}

void ChessApp::handleRematchRequestMessage()
{
    openRematchRequestWindow();
    closeWinLossDrawPopup();
}

void ChessApp::handleRematchAcceptMessage()
{
    m_board.resetBoard();
}

//called once per frame at the beginning of the frame in ChessApp::run()
void ChessApp::processIncomingMessages()
{
    if(!m_netWork.isUserConnected())
        return;

    if(auto result = m_netWork.recieveMessageIfAvailable())//std::optional operator bool() called
    {
        auto& msg = result.value();

        //the first byte of a message sent is always the type of
        //the message encoded as a simple integer macro defined in ChessNetworking.h
        int msgType = static_cast<int>(msg.at(0));

        switch(msgType)
        {
        case MOVE_MSGTYPE:             handleMoveMessage(msg);       break;
        case RESIGN_MSGTYPE:           handleResignMessage();        break;
        case DRAW_OFFER_MSGTYPE:       handleDrawOfferMessage();     break;
        case REMATCH_ACCEPT_MSGTYPE:   handleRematchAcceptMessage(); break;
        case PAIR_REQUEST_MSGTYPE:     handleRematchRequestMessage();
        }
    }

    //if we are no longer connected as indicated by the call to recv() returning 0,
    //and recieveMessageIfAvailable() changing P2PChessConnection::m_connectType to INVALID.
    if(!m_netWork.isUserConnected())
    {
        //if we are no longer connected...
        m_board.resetBoard();
        m_board.setBoardViewingPerspective(Side::WHITE);
    }
}

//tells if a chess position is on the board or not
bool ChessApp::inRange(Vec2i const chessPos)
{
    return chessPos.x <= 7 && chessPos.x >= 0 && chessPos.y <= 7 && chessPos.y >= 0;
}

void ChessApp::drawPromotionPopup()
{
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

    bool wasPromoACapture = !!m_board.getLastCapturedPiece();
    Move promoMove = m_board.getLastMove();//get the move made by the pawn to promote
    assert(promoMove.m_moveType == MoveInfo::PROMOTION || 
        promoMove.m_moveType == MoveInfo::ROOK_CAPTURE_AND_PROMOTION);
    Vec2i promotionSquare = promoMove.m_dest;
    Vec2i promotionScreenPos = chess2ScreenPos(promotionSquare);
    promotionScreenPos.x -= m_squareSize * 0.5f; promotionScreenPos.y -= m_squareSize * 0.5f;
    if(m_board.getViewingPerspective() != m_board.getWhosTurnItIs())
        promotionScreenPos.y -= m_squareSize * 3;

    ImGui::SetWindowPos(promotionScreenPos);

    auto indecies = (m_board.getWhosTurnItIs() == Side::WHITE) ? std::array{WQUEEN, WROOK, WKNIGHT, WBISHOP} :
        std::array{BQUEEN, BROOK, BKNIGHT, BBISHOP};

    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[0]]), m_pieceTextureSizes[indecies[0]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        m_board.postMoveUpdate(promoMove, PromoType::QUEEN_PROMOTION); closePromotionPopup();
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[1]]), m_pieceTextureSizes[indecies[1]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        m_board.postMoveUpdate(promoMove, PromoType::ROOK_PROMOTION); closePromotionPopup();
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[2]]), m_pieceTextureSizes[indecies[2]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        m_board.postMoveUpdate(promoMove, PromoType::KNIGHT_PROMOTION); closePromotionPopup();
    }
    if(ImGui::ImageButton(static_cast<ImTextureID>(m_pieceTextures[indecies[3]]), m_pieceTextureSizes[indecies[3]],
       {0, 0}, {1, 1}, -1, {0, 0, 0, 0}, {1, 1, 1, 0.25f}))
    {
        m_board.postMoveUpdate(promoMove, PromoType::BISHOP_PROMOTION); closePromotionPopup();
    }

    ImGui::End();
    ImGui::PopStyleVar(4);
}

void ChessApp::drawRematchRequestWindow()
{
    ImGui::Begin("opponent has requested rematch", &m_wnd.m_rematchRequestWindowIsOpen);

    ImGui::TextUnformatted("your opponent has requested a rematch");

    if(ImGui::Button("accept"))
    {
        m_board.resetBoard();
        send1ByteMessage(ChessConnection::NetMessageType::REMATCH_ACCEPT);
        closeRematchRequestWindow();
    }

    if(ImGui::Button("decline and disconnect"))
    {
        m_netWork.disconnect();
        m_board.resetBoard();
        closeRematchRequestWindow();
    }

    ImGui::End();
}

//tells wether mouse position is over a given rectangle
bool ChessApp::isMouseOver(SDL_Rect const& r)
{
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    return(x >= r.x && x <= r.x+r.w && y >= r.y && y <= r.h + r.y);
}

std::string ChessApp::getCurrentDateAndTime()
{
    auto const now = std::chrono::system_clock::now();
    std::time_t const time = std::chrono::system_clock::to_time_t(now);
    char dateAndTime[256]{};
    ctime_s(dateAndTime, 256, &time);
    return std::string(dateAndTime);
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

    //move down to account for the menu bar.
    //will only work after the first frame since the first imgui frame
    //needs to be started to measure the menu bar
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

//return if the position is on the board (false if over an imgui window)
//in the future I will add a border with the rank file indicators around the board
//and this func will also return false when the mouse is over that border.
bool ChessApp::isScreenPositionOnBoard(Vec2i const& screenPosition)
{
    return !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
}

void ChessApp::drawMoveIndicatorCircles()
{
    auto const pom = Piece::getPieceOnMouse();
    if(!pom) return;

    SDL_Renderer *const renderer  = ChessApp::getCurrentRenderer();

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
        auto const piece = m_board.getPieceAt(move.m_dest);
        if(piece && piece->getSide() != m_board.getWhosTurnItIs() ||
            (move.m_dest == m_board.getEnPassantIndex()) && std::dynamic_pointer_cast<Pawn>(pom))
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

void ChessApp::drawPiecesNotOnMouse()
{
    auto const& pom = Piece::getPieceOnMouse();

    for(auto const piece : s_theApplication.m_board.getPieces())
    { 
        if(piece && piece != pom)
        {
            //figure out where on the screen the piece is (the middle of the square)
            Vec2i screenPosition = chess2ScreenPos(piece->getChessPosition());
            
            //get the index into the array of piece textures for this piece
            auto idx = piece->getWhichTexture();

            //get the width and height of whichever texture the piece on the mouse is
            Vec2i textureSize = m_pieceTextureSizes[idx];
            
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
                getCurrentRenderer(), 
                m_pieceTextures[idx],
                nullptr, &destination
            );
        }
    }
}

void ChessApp::drawPieceOnMouse()
{
    auto const pom = Piece::getPieceOnMouse();
    if(pom)
    {
        Vec2i mousePosition{};
        SDL_GetMouseState(&mousePosition.x, &mousePosition.y);
        Vec2i textureSize = m_pieceTextureSizes[pom->getWhichTexture()];
        SDL_Rect destination
        {
            .x = mousePosition.x - textureSize.x / 2,
            .y = mousePosition.y - textureSize.y / 2,
            .w = textureSize.x, 
            .h = textureSize.y
        };
        SDL_RenderCopy
        (
            getCurrentRenderer(), 
            m_pieceTextures[pom->getWhichTexture()],
            nullptr,
            &destination
        );
    }
}

void ChessApp::loadPieceTexturesFromDisk()
{   
    SDL_Renderer* renderer = ChessApp::getCurrentRenderer();
    if(!(m_pieceTextures[WPAWN]   = IMG_LoadTexture(renderer, "textures/wPawn.png"))) 
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[WKNIGHT] = IMG_LoadTexture(renderer, "textures/wKnight.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[WROOK]   = IMG_LoadTexture(renderer, "textures/wRook.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[WBISHOP] = IMG_LoadTexture(renderer, "textures/wBishop.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[WQUEEN]  = IMG_LoadTexture(renderer, "textures/wQueen.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[WKING]   = IMG_LoadTexture(renderer, "textures/wKing.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[BPAWN]   = IMG_LoadTexture(renderer, "textures/bPawn.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[BKNIGHT] = IMG_LoadTexture(renderer, "textures/bKnight.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[BROOK]   = IMG_LoadTexture(renderer, "textures/bRook.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[BBISHOP] = IMG_LoadTexture(renderer, "textures/bBishop.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[BQUEEN]  = IMG_LoadTexture(renderer, "textures/bQueen.png")))
        throw std::runtime_error(IMG_GetError());
    if(!(m_pieceTextures[BKING]   = IMG_LoadTexture(renderer, "textures/bKing.png")))
        throw std::runtime_error(IMG_GetError());

    //get the width and height of each texture
    for(int i = 0; i < NUM_OF_PIECE_TEXTURES; ++i)
    {
        SDL_QueryTexture(m_pieceTextures[i], nullptr, nullptr, 
            &m_pieceTextureSizes[i].x, &m_pieceTextureSizes[i].y);

        //scale the texture up or down to the correct size
        m_pieceTextureSizes[i] *= m_pieceTextureScale;
    }
}
