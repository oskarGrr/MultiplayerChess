#include <functional>
#include <unordered_map>
#include <typeindex>

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
    void subscribe(OnEventCallback callback)
    {
        //static assert used instead of a requires constraint to allow for a helpful error message.
        static_assert(IsTypeInPack<EventType, EventTs...>, "The event type template paramater passed to"
            " EventSystem::subscribe is not a valid event type for this EventSystem." 
            " The list of valid event types are passed in to the variadic class template EventSystem");

        auto& functions {mSubscriptions.try_emplace(typeid(EventType)).first->second};
        functions.push_back(std::move(callback));
    }

    template <typename EventType>
    void publish(EventType& e)
    {
        //static assert used instead of a requires constraint to allow for a helpful error message.
        static_assert(IsTypeInPack<EventType, EventTs...>, "The event type template paramater passed to"
            " EventSystem::publish is not a valid event type for this EventSystem." 
            " The list of valid event types are passed in to the variadic class template EventSystem");

        auto const it = mSubscriptions.find(typeid(e));

        if(it != mSubscriptions.end())
        {
            for(auto const& fn : it->second)
                fn(e);
        }
    }

    std::unordered_map< std::type_index, std::vector<OnEventCallback> > mSubscriptions;
};