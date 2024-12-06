#include "PopupManager.hpp"
#include "imgui.h"

void PopupManager::drawPopup()
{
    if( ! mIsPopupOpen ) { return; }

    //returns true if the popup should close
    auto const checkButtons = [this]() -> bool
    {
        std::string const popupTxt {mCurrentPopup.text};

        for(auto const& button : mCurrentPopup.buttons)
        {
            if(ImGui::Button(button.text.c_str()))
            {
                //the callback returned true meaning this popup should close
                if(button.callback())
                {
                    //actually close if the text of the popup did not change
                    return popupTxt == mCurrentPopup.text;
                }
            }

            ImGui::SameLine();
        }

        return false;
    };
    
    //put the popup in the center of the screen
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});

    ImGui::OpenPopup(mCurrentPopup.text.c_str());

    if(ImGui::BeginPopup(mCurrentPopup.text.c_str()))
    {
        ImGui::TextUnformatted(mCurrentPopup.text.c_str());

        if(checkButtons())
        {
            mIsPopupOpen = false;
            mCurrentPopup.buttons.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PopupManager::startNewPopup(std::string popupText, bool shouldHaveOkayButton)
{
    mCurrentPopup.text = std::move(popupText);

    mCurrentPopup.buttons.clear();

    if(shouldHaveOkayButton)
        mCurrentPopup.buttons.emplace_back("Okay", []{return true;});

    mIsPopupOpen = true;
}