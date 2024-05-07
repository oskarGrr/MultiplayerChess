#pragma once
#include <cstdint>

//this just holds some inline stuff for the castling rights stuff
//so that it doesnt clutter up another header file

//bit masks to indicate what castling rights are available.
enum CastleRights : uint32_t
{
    NONE = 0, WSHORT = 0b1, WLONG = 0b10, BSHORT = 0b100, BLONG = 0b1000
};

inline CastleRights operator|(CastleRights const lhs, CastleRights const rhs)
{
    return static_cast<CastleRights>(static_cast<uint32_t>(lhs) | 
        static_cast<uint32_t>(rhs));
}

inline CastleRights& operator|=(CastleRights& lhs, CastleRights const rhs)
{
    return lhs = static_cast<CastleRights>(lhs | rhs);
}

inline CastleRights operator&(CastleRights const lhs, CastleRights const rhs)
{
    return static_cast<CastleRights>(static_cast<uint32_t>(lhs) &
        static_cast<uint32_t>(rhs));
}

inline CastleRights& operator&=(CastleRights& lhs, CastleRights const rhs)
{
    return lhs = static_cast<CastleRights>(lhs & rhs);
}