#pragma once
#include <type_traits>//std::same_as std::decay_t
#include <utility>//std::forward
#include <string_view>
#include <vector>
#include <functional>
#include <string>

//Simple class that encapsulates the ability to draw Dear Imgui popups.
class PopupManager
{
public:

    void draw();

    //if shouldHaveOkayButton is true, then the popup will have a button 
    //that says "Okay" and simply closes the popup.
    void startNewPopup(std::string popupText, bool shouldHaveOkayButton);

    struct Button
    {
        std::string text;//the text inside the button
        std::function<bool(void)> callback;//callback returns true to close the popup
    };

    void addButton(Button&& b) { mCurrentPopup.buttons.push_back(b); }

private:

    struct Popup
    {
        std::string text; //The main information text that is displayed in the popup.
        std::vector<Button> buttons;
    };

    bool mIsPopupOpen {false};
    Popup mCurrentPopup;

public:

    PopupManager()=default;
    ~PopupManager()=default;

    PopupManager(PopupManager const&)=delete;
    PopupManager(PopupManager&&)=delete;
    PopupManager& operator=(PopupManager const&)=delete;
    PopupManager& operator=(PopupManager&&)=delete;
};