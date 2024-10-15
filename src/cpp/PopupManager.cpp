#include "PopupManager.hpp"
#include "imgui.h"

void PopupManager::drawPopups()
{
    //return true if the popup should close
    auto const checkButtons = [](auto const& buttons) -> bool
    {
        for(auto const& button : buttons)
        {
            if(ImGui::Button(button.text.c_str()))
            {
                if(button.callback())//the callback returned true meaning this popup should close
                    return true;
            }

            ImGui::SameLine();
        }

        return false;
    };

    auto const wndFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    
    for(auto it = mOpenPopups.begin(); it != mOpenPopups.end(); /*increment it at end of loop scope*/)
    {
        centerNextWindow();
        ImGui::OpenPopup(it->text.c_str());   

        if(ImGui::BeginPopup(it->text.c_str()), wndFlags)
        {
            ImGui::TextUnformatted(it->text.c_str());

            if(checkButtons(it->buttons))
            {
                ImGui::CloseCurrentPopup();
                it = mOpenPopups.erase(it);
                ImGui::EndPopup();
                continue;
            }

            ImGui::EndPopup();
        }

        ++it;
    }
}

void PopupManager::centerNextWindow()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
}