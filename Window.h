#pragma once
#include "SDL.h"
class chessApp;

//simple wrapper that initializes an SDL window as well as imgui things
class Window
{
private:

    friend class ChessApp;//only chess app should make windows
    Window(int const width, int const height, 
        char const* title, Uint32 const SDL_subsystems, Uint32 const SDL_windowFlags);
    ~Window();//cleanup SDL and imGui

    Window(Window const&)=delete;
    Window(Window&&)=delete;
    Window& operator=(Window const&)=delete;
    Window& operator=(Window&&)=delete;

    SDL_Renderer* m_renderer;
    SDL_Window* m_window;
    int const m_width, m_height;
    bool m_ColorEditorWindowIsOpen;
    bool m_demoWindowIsOpen;
    bool m_promotionWindowIsOpen;
    bool m_connectWindowIsOpen;
};