#include "Window.h"
#include "SDL.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

Window::Window(int const width, int const height, 
               char const* title, Uint32 const SDL_subsystems,
               Uint32 const SDL_windowFlags)
    : m_renderer(nullptr), m_window(nullptr), m_width(width), m_height(height),
      m_ColorEditorWindowIsOpen(false), m_demoWindowIsOpen(false), 
      m_promotionWindowIsOpen(false), m_connectWindowIsOpen(false), 
      m_resetBoardPopupIsOpen(false), m_newConnectionPopupIsOpen(false),
      m_winLossPopupIsOpen(false)
{
    SDL_Init(SDL_subsystems);
    SDL_CreateWindowAndRenderer(width, height, SDL_windowFlags, &m_window, &m_renderer);//set m_window && m_renderer
    SDL_SetWindowTitle(m_window, title);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer_Init(m_renderer);

    //add the font from the fonts folder
    auto& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf", 16.0); 

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 9.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    auto& style = ImGui::GetStyle();
    style.FramePadding = {5.0f, 3.0f};
    style.DisplaySafeAreaPadding = {0.0f, 5.0f};
}

//cleanup SDL and imGui
Window::~Window()
{
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}