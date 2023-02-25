#pragma once
#include <iostream>
#include "imgui.h"

//used for various things such as an invalid/null chess position
#define INVALID_VEC2I Vec2i{-1, -1}

//inline vector2 struct used for chess positions (0-7)
struct Vec2i
{
    int x = 0, y = 0;

    //defaulted c++20 spaceship operator allows compiler 
    //to supply default comparison operators for this struct
    auto operator<=>(Vec2i const&) const = default;

    inline operator ImVec2() const
    {
        return ImVec2(static_cast<float>(x), static_cast<float>(y));
    }

    friend std::ostream& operator<<(std::ostream& os, Vec2i const& v)
    {
        std::cout << v.x << ", " << v.y; 
        return os;
    }

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