#pragma once
#include <cstdint>
#include <bitset>
#include "chessNetworkProtocol.h" //enum Side

//this just holds some inline stuff for the castling rights stuff
//so that it doesnt clutter up another header file

class CastleRights
{
public:
    enum struct Rights { WSHORT, WLONG, BSHORT, BLONG };

    bool hasRights(Rights) const;
    bool hasRights(Side whiteOrBlack) const;
    void revokeRights(Rights rights);
    void revokeRights(Side whiteOrBlack);
    void addRights(Rights);
    void addRights(Side whiteOrBlack);
    
private:
    std::bitset<4> mCastleRightsBits;
};