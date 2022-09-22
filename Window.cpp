#include "Window.h"
#include "SDL.h"

Window::Window(int const width, int const height, 
    char const* title, Uint32 const SDL_subsystems, Uint32 const SDL_windowFlags)
    : m_renderer(nullptr), m_window(nullptr), m_width(width), m_height(height)
{
    SDL_Init(SDL_subsystems);
    SDL_CreateWindowAndRenderer(width, height, SDL_windowFlags, &m_window, &m_renderer);
    SDL_SetWindowTitle(m_window, title);
}

Window::~Window()
{
    SDL_DestroyWindow(m_window);
    SDL_DestroyRenderer(m_renderer);
    SDL_Quit();
}