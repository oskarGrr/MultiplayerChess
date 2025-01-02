#include "PopupManager.hpp"
#include "imgui.h"

struct RAIIPopupstyles
{
    RAIIPopupstyles()
    {
        pushColor(ImGuiCol_Text, {0.0f, 0.0f, 0.0f, 1.0f});
        pushColor(ImGuiCol_Button, {.2f, 0.79f, 0.8f, 0.70f});
        pushColor(ImGuiCol_ButtonHovered, {.011f, 0.615f, 0.988f, .75f});
        pushColor(ImGuiCol_PopupBg, {.9f, .9f, .9f, .95f});
    }

    ~RAIIPopupstyles()
    {
        ImGui::PopStyleColor(numColors);
        ImGui::PopStyleVar(numStyles);
    }

private:

    void pushColor(ImGuiCol colorIdx, ImVec4 const& color)
    {
        ImGui::PushStyleColor(colorIdx, color);
        ++numColors;
    }

    void pushStyle(ImGuiStyleVar styleVar, float val)
    {
        ImGui::PushStyleVar(styleVar, val);
        ++numStyles;
    }

    void pushStyle(ImGuiStyleVar styleVar, ImVec2 const& val)
    {
        ImGui::PushStyleVar(styleVar, val);
        ++numStyles;
    }

    int numColors {0};
    int numStyles {0};
};

void PopupManager::draw()
{
    if( ! mIsPopupOpen ) { return; }

    [[maybe_unused]] RAIIPopupstyles styles; //RAII style push and pop imgui colors and styles

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

    auto const windowFlags 
    {
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_AlwaysAutoResize
    };

    if(ImGui::BeginPopupModal(mCurrentPopup.text.c_str(), nullptr, windowFlags))
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