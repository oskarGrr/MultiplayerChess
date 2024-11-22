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
    
    void drawPopups();

    struct Popup
    {
        struct Button
        {
            std::string text;//the text inside the button
            std::function<bool(void)> callback;//callback returns true to close the popup
        };

        std::string text; //The main information text that is displayed in the popup.
        std::vector<Button> buttons;
    };

    template <typename P> requires std::same_as<std::decay_t<P>, Popup>
    void openPopup(P&& popup) //forwarding ref rvalue or lvalue for the correct push_back() overload
    {
        mOpenPopups.push_back(popup);
    }

private:

    std::vector<Popup> mOpenPopups;
    void centerNextWindow();

public:
    PopupManager()=default;
    ~PopupManager()=default;
    PopupManager(PopupManager const&)=delete;
    PopupManager(PopupManager&&)=delete;
    PopupManager& operator=(PopupManager const&)=delete;
    PopupManager& operator=(PopupManager&&)=delete;
};