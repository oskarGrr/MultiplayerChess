#include <chrono>
#include <exception>
#include "imgui_impl_sdl.h"
#include "SDL.h"
#include "ChessEvents.hpp"
#include "errorLogger.hpp"
#include "Board.hpp"
#include "ChessRenderer.hpp"
#include "ConnectionManager.hpp"
#include "SoundManager.hpp"

static void runApplication();

int main(int argumentCount, char** argumentVector)
{
    try
    {
        runApplication();
    }
    catch(std::exception& e)
    {
        std::string errMsg{e.what()};
        errMsg.append(" (caught in main())");
        FileErrorLogger::get().log(errMsg);
        return EXIT_FAILURE;
    }
    catch(...)
    {
        FileErrorLogger::get().log("exception caught of unknown type in main()");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void handleLeftClickPressSDLEvent(Board&, ConnectionManager const&, 
    AppEventSystem::Publisher const& appEventPublisher);

void handleLeftClickReleaseSDLEvent(Board&, ChessRenderer const&, 
    AppEventSystem::Publisher const& appEventPublisher);

static void runApplication()
{
    NetworkEventSystem networkEventSys;
    GUIEventSystem guiEventSys;
    BoardEventSystem boardEventSys;
    AppEventSystem appEventSys;

    ConnectionManager connectionManager {networkEventSys.getPublisher(), guiEventSys.getSubscriber(), 
        boardEventSys.getSubscriber()};

    ChessRenderer chessRenderer {networkEventSys.getSubscriber(), boardEventSys.getSubscriber(), 
        guiEventSys.getPublisher(), appEventSys.getSubscriber()};

    SoundManager soundManager {boardEventSys.getSubscriber()};

    Board board {boardEventSys.getPublisher(), guiEventSys.getSubscriber(), 
        networkEventSys.getSubscriber(), appEventSys.getSubscriber()};

    bool appRunning {true};
    //double deltaTime {0.0};//unused for now

    while(appRunning)
    {
        //auto const start {std::chrono::steady_clock::now()};

        connectionManager.update();

        SDL_Event evnt;
        while(SDL_PollEvent(&evnt))
        {
            ImGui_ImplSDL2_ProcessEvent(&evnt);

            switch(evnt.type)
            {
            case SDL_QUIT:
            {
                appRunning = false;
                break;
            }
            case SDL_MOUSEBUTTONDOWN: 
            {
                if(evnt.button.button == SDL_BUTTON_LEFT)
                    handleLeftClickPressSDLEvent(board, connectionManager, appEventSys.getPublisher());

                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                if(evnt.button.button == SDL_BUTTON_LEFT)
                    handleLeftClickReleaseSDLEvent(board, chessRenderer, appEventSys.getPublisher());
            }
            }
        }

        chessRenderer.render(board, connectionManager);
        SDL_Delay(10);

        //auto const end { std::chrono::steady_clock::now() };
        //deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1.0E9;
    }
}

static void handleLeftClickReleaseSDLEvent(Board& board, ChessRenderer const& chessRenderer, 
    AppEventSystem::Publisher const& appEventPublisher)
{
    int mx{}, my{};
    SDL_GetMouseState(&mx, &my);

    if(auto maybeChessPos { chessRenderer.screen2ChessPos({mx, my}) })
    {
        AppEvents::LeftClickRelease evnt {*maybeChessPos};
        appEventPublisher.pub(evnt);
    }
}

static void handleLeftClickPressSDLEvent(Board& board, ConnectionManager const& connectionManager, 
    AppEventSystem::Publisher const& appEventPublisher)
{
    //if we are playing online and it is not our turn
    if(connectionManager.isPairedOnline() && board.getSideUserIsPlayingAs() != board.getWhosTurnItIs())
        return;

    int x{0}, y{0};
    SDL_GetMouseState(&x, &y);

    //post a left click event indicating that we are attempting to grab a chess piece
    AppEvents::LeftClickPress evnt(x, y);
    appEventPublisher.pub(evnt);
}