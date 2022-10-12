#pragma once
#include "SDL.h"
class chessApp;

class Window
{
private:

    friend class ChessApp;//only chess app should make winsows
    Window(int const width, int const height, 
        char const* title, Uint32 const SDL_subsystems, Uint32 const SDL_windowFlags);
    ~Window();

    Window(Window const&)=delete;
    Window(Window&&)=delete;
    Window& operator=(Window const&)=delete;
    Window& operator=(Window&&)=delete;

    SDL_Renderer* m_renderer;
    SDL_Window* m_window;
    int const m_width, m_height;
    bool m_ColorEditorWindowIsOpen;
    bool m_demoWindowIsOpen;
};