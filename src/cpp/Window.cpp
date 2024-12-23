#include "Window.hpp"
#include "SDL.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <exception>

Window::Window(int const width, int const height,
               char const* title, uint32_t const SDL_subsystemFlags,
               uint32_t const SDL_windowFlags)
{
    if(SDL_Init(SDL_subsystemFlags)) 
        throw std::exception(SDL_GetError());

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_windowFlags);
    if( ! window ) { throw std::exception(SDL_GetError()); }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if( ! renderer ) { throw std::exception(SDL_GetError()); }

    SDL_SetWindowTitle(window, title);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsLight();
    
    auto& style { ImGui::GetStyle() };
    style.Colors[ImGuiCol_WindowBg]  = {0.11f, 0.11f, 0.12f, 1.0f};
    style.Colors[ImGuiCol_Text]      = {0.94f, 0.94f, 0.94f, 1.0f};
    style.Colors[ImGuiCol_MenuBarBg] = {0.58f, 0.61f, 0.61f, 1.0f};

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    //add the font from the fonts folder
    auto& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf", 16.0); 
}

//cleanup SDL and imGui
Window::~Window()
{
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}