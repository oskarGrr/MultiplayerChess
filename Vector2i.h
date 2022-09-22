#pragma once
#include <iostream>

//inline vector2 struct used for chess positions (0-7)
struct Vec2i
{
    int x = 0, y = 0;

    auto operator<=>(Vec2i const&) const = default;

    inline void printAsChessPos() const 
    {
        std::cout << char(x + 97) << (y + 1);
    }

    inline Vec2i operator-(Vec2i const& rhs) const
    {
        return {x - rhs.x, y - rhs.y};
    }

    inline Vec2i operator+(Vec2i const& rhs) const
    {
        return {x + rhs.x, y + rhs.y};
    }

    inline Vec2i& operator+=(Vec2i const& rhs)
    {
        *this = *this + rhs;
        return *this;
    }

    inline Vec2i operator*(int const scaler) const
    {
        return {x * scaler, y * scaler};
    }

    inline Vec2i& operator*=(int const scaler)
    {
        *this = *this * scaler;
        return *this;
    }
};

inline std::ostream& operator<<(std::ostream& os, Vec2i const& v)
{
    std::cout << v.x << ", " << v.y; 
    return os;
}
