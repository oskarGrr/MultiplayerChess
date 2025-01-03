#include "Window.hpp"
#include "SDL.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "errorLogger.hpp"
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
    style.Colors[ImGuiCol_WindowBg]  = {0.164f, 0.168f, 0.172f, 1.0f};
    style.Colors[ImGuiCol_Text]      = {0.94f, 0.94f, 0.94f, 1.0f};
    style.Colors[ImGuiCol_MenuBarBg] = {0.49f, 0.53f, 0.53f, 1.00f};
    style.Colors[ImGuiCol_FrameBg]   = {0.423f, 0.474f, 0.470f, 1.0f};

    style.WindowRounding = 0.0f;

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    //add the font from the fonts folder
    auto& io = ImGui::GetIO();
    if( ! io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf", 16.0) )
        FileErrorLogger::get().log("failed to load fonts/DroidSans.ttf");
}

//cleanup SDL and imGui
Window::~Window()
{
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}