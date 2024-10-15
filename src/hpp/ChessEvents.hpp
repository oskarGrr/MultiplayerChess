#pragma once
#include "ChessMove.hpp"
#include "chessNetworkProtocol.h" //enum Side
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <string>

//no virtual dtor because runtime polymorphism is not used
struct Event {};

template <typename T, typename... Types>
concept IsTypeInPack = (std::is_same_v<T, Types> || ...);

template <typename... EventTs>
class EventSystem
{
public:

    static_assert((std::is_base_of_v<Event, EventTs> && ...),
        "All event types must inherit from Event (no runtime polymorphism)");

    using OnEventCallback = std::function<void(Event&)>;

    template <typename EventType>
    void sub(OnEventCallback callback)
    {
        static_assert
        (
            IsTypeInPack<EventType, EventTs...>, 
            "The template type paramater passed to"
            " EventSystem::sub was not a valid event type for this EventSystem."
        );
        
        mSubscriptions[typeid(EventType)].push_back(std::move(callback));
    }

    template <typename EventType>
    void pub(EventType& e) const
    {
        static_assert
        (
            IsTypeInPack<EventType, EventTs...>, 
            "The template type paramater passed to"
            " EventSystem::pub was not a valid event type for this EventSystem."
        );

        auto const it { mSubscriptions.find(typeid(e)) };

        if(it != mSubscriptions.end())
        {
            for(auto const& fn : it->second)
                fn(e);
        }
    }

private:

    std::unordered_map< std::type_index, std::vector<OnEventCallback> > mSubscriptions;
};

//Events that occur when the user presses a button/input text in the GUI
namespace GUIEvents
{
    struct ResetBoard : Event {};
    struct RematchRequest : Event {};
    struct DrawAccepted : Event {};
    struct DrawDeclined : Event {};
    struct DrawRequest : Event {};
    struct Unpair : Event {}; //When the user presses the "disconnect from opponent" button at the end of an online game
    struct RematchAccept : Event {};
    struct RematchDecline : Event {};
    struct PairAccept : Event {};
    struct PairDecline : Event {};
    struct Resign : Event {};
    struct PairRequest : Event {uint32_t opponentID{};};
    struct PromotionEnd : Event {ChessMove::PromoTypes promoType;};
}

using GUIEventSystem = EventSystem
<
    GUIEvents::ResetBoard,
    GUIEvents::DrawAccepted,
    GUIEvents::DrawDeclined,
    GUIEvents::RematchRequest,
    GUIEvents::Unpair, 
    GUIEvents::RematchAccept,
    GUIEvents::PairDecline,
    GUIEvents::RematchDecline,
    GUIEvents::Resign,
    GUIEvents::DrawRequest,
    GUIEvents::PairRequest,
    GUIEvents::PairAccept,
    GUIEvents::PromotionEnd
>;

namespace BoardEvents
{
    struct GameOver : Event {std::string reason; bool isPaired{false};};
    struct PromotionBegin : Event {Side promotingSide; Vec2i promotingSquare;};
    struct MoveCompleted : Event {ChessMove move;};
}

using BoardEventSystem = EventSystem
<
    BoardEvents::GameOver,
    BoardEvents::PromotionBegin,
    BoardEvents::MoveCompleted
>;

namespace IncommingNetworkEvents
{
    struct OpponentMadeMove : Event {ChessMove move;};
    struct DrawOffer : Event {};
    struct DrawDeclined : Event {};
    struct PairRequest : Event {uint32_t potentialOpponentID{};};
    struct RematchRequest : Event {};
    struct RematchAccept : Event {};
    struct RematchDecline : Event {};
    struct PairingComplete : Event {uint32_t opponentID{}; Side side{Side::INVALID};};
    struct OpponentClosedConnection : Event {};
    struct Unpair : Event {};
    struct NewID : Event {uint32_t newID{};};
    struct IDNotInLobby : Event {uint32_t ID{};};
    struct PairDecline : Event {};
    struct DrawAccept : Event {};
    struct OpponentHasResigned : Event{};
}

using IncommingNetworkEventSystem = EventSystem
<
    IncommingNetworkEvents::OpponentMadeMove,
    IncommingNetworkEvents::DrawOffer, 
    IncommingNetworkEvents::DrawDeclined,
    IncommingNetworkEvents::PairRequest,
    IncommingNetworkEvents::RematchRequest,
    IncommingNetworkEvents::RematchAccept,
    IncommingNetworkEvents::RematchDecline,
    IncommingNetworkEvents::PairingComplete,
    IncommingNetworkEvents::OpponentClosedConnection,
    IncommingNetworkEvents::Unpair,
    IncommingNetworkEvents::NewID,
    IncommingNetworkEvents::IDNotInLobby,
    IncommingNetworkEvents::PairDecline,
    IncommingNetworkEvents::DrawAccept,
    IncommingNetworkEvents::OpponentHasResigned
>;