#include <chrono>
#include <exception>
#include "imgui_impl_sdl.h"
#include "SDL.h"
#include "ChessEvents.hpp"
#include "errorLogger.h"
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

void handleLeftClickPressEvent(Board&, ConnectionManager const&, ChessRenderer const&);
void handleLeftClickReleaseEvent(Board&, ChessRenderer const&);

static void runApplication()
{
    IncommingNetworkEventSystem incNetworkEventSys;
    GUIEventSystem guiEventSys;
    BoardEventSystem boardEventSys;

    ConnectionManager connectionManager {incNetworkEventSys, guiEventSys, boardEventSys};
    ChessRenderer chessRenderer {incNetworkEventSys, boardEventSys, guiEventSys};
    SoundManager soundManager {boardEventSys};
    Board board {boardEventSys, guiEventSys, incNetworkEventSys};

    bool appRunning {true};
    double deltaTime {0.0};
    while(appRunning)
    {
        auto const start {std::chrono::steady_clock::now()};

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
                    handleLeftClickPressEvent(board, connectionManager, chessRenderer);

                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                if(evnt.button.button == SDL_BUTTON_LEFT)
                    handleLeftClickReleaseEvent(board, chessRenderer);
            }
            }
        }

        chessRenderer.renderAllTheThings(board, connectionManager);
        SDL_Delay(10);

        auto const end {std::chrono::steady_clock::now()};
        deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1.0E9;
    }
}

static void handleLeftClickReleaseEvent(Board& board, ChessRenderer const& chessRenderer)
{
    int mx{}, my{};
    SDL_GetMouseState(&mx, &my);
    board.putPieceDown(chessRenderer.screen2ChessPos({mx, my}));
}

static void handleLeftClickPressEvent(Board& board, ConnectionManager const& connectionManager, 
    ChessRenderer const& chessRenderer)
{
    int x, y;
    SDL_GetMouseState(&x, &y);

    if( ! chessRenderer.isScreenPositionOnBoard({x, y}) )
        return;

    //If we are still waiting for the online opponent to make a move
    if(connectionManager.isPairedOnline() && board.getSideUserIsPlayingAs() != board.getWhosTurnItIs())
        return;

    board.pickUpPiece(chessRenderer.screen2ChessPos({x, y}));
}