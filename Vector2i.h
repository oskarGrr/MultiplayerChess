#pragma once
#include <iostream>

//inline vector2 struct used for chess positions (0-7)
struct Vec2i
{
    int x = 0, y = 0;

    //defaulted c++20 spaceship operator allows compiler 
    //to supply default comparison operators for this struct
    auto operator<=>(Vec2i const&) const = default;

    inline void printAsChessPos() const 
    {
        std::cout << char(x + 97) << (y + 1);
    }

    friend std::ostream& operator<<(std::ostream& os, Vec2i const& v)
    {
        std::cout << v.x << ", " << v.y; 
        return os;
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

    inline Vec2i operator*(float const scaler) const
    {
        return {static_cast<int>(x * scaler), static_cast<int>(y * scaler)};
    }

    inline Vec2i& operator*=(float const scaler)
    {
        *this = *this * scaler;
        return *this;
    }
};