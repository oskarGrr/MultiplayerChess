#pragma once
#include <iostream>
#include "imgui.h"

//inline vector2 struct used for chess positions (0-7)
struct Vec2i
{
    int x{0}, y{0};

    constexpr Vec2i() = default;
    constexpr Vec2i(int x_, int y_) : x{x_}, y{y_} {}
    constexpr Vec2i(ImVec2 imVec2) : x{static_cast<int>(imVec2.x)}, y{static_cast<int>(imVec2.y)} {}

    //Defaulted c++20 spaceship operator allows compiler 
    //to supply default comparison operators for this struct.
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

    inline Vec2i& operator-=(Vec2i const& rhs)
    {
        *this = *this - rhs;
        return *this;
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

    inline Vec2i operator/(float const rhs)
    {
        return {static_cast<int>(x / rhs), static_cast<int>(y / rhs)};
    }

    inline Vec2i operator%(int const rhs)
    {
        return {x % rhs, y % rhs};
    }
};

//used for various things such as an invalid/null chess position
static constexpr Vec2i INVALID_VEC2I{-1, -1};