#pragma once
#include <string>
#include <string_view>
#include <functional>
#include <utility>//std::move() std::pair
#include <typeindex>
#include <unordered_map>

struct Event
{
protected:
    virtual ~Event()=default;
};

template <typename T>
concept ChessEvent = std::is_base_of_v<Event, T>;

//Extremely simple syncronous publisher/subscriber event system.
//Events are not put in a queue. They are handled immediately.
class PubSubEventSystem
{
public:

    using OnEventCallback = std::function<void(Event&)>;

    template <ChessEvent>
    void subscribe(OnEventCallback callback)
    {
        //Get a reference to the vector of functions for this event type
        auto const& functions {mSubscriptions.try_emplace(
            typeid(IsChessEvent)).first->second};

        functinos.push_back(std::move(callback));
    }

    template <ChessEvent EventType>
    void publish(EventType& evnt)
    {
        auto const it = mSubscriptions.find(typeid(evnt));

        if(it != mSubscriptions.end())
        {
            for(auto const& fn : it->second)
                fn(evnt);
        }
    }

private:

    std::unordered_map<std::type_index, std::vector<OnEventCallback>> mSubscriptions;

public:
    PubSubEventSystem()=default;
    ~PubSubEventSystem()=default;
    PubSubEventSystem(PubSubEventSystem const&)=delete;
    PubSubEventSystem(PubSubEventSystem&&)=delete;
    PubSubEventSystem& operator=(PubSubEventSystem const&)=delete;
    PubSubEventSystem& operator=(PubSubEventSystem&&)=delete;
};