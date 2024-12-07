#pragma once
#include <cstdint>
#include <bitset>
#include "chessNetworkProtocol.h" //enum Side

//this just holds some inline stuff for the castling rights stuff
//so that it doesnt clutter up another header file

class CastleRights
{
public:
    enum struct Rights : uint8_t { WSHORT, WLONG, BSHORT, BLONG };

    CastleRights()=default;
    CastleRights(unsigned char rights);

    bool hasRights(Rights) const;
    bool hasRights(Side whiteOrBlack) const;

    void revokeRights(Rights rights);
    void revokeRights(Side whiteOrBlack);
    void revokeRights(CastleRights const& rightsToRevoke);

    void addRights(Rights);
    void addRights(Side whiteOrBlack);
    void addRights(CastleRights const& rightsToAdd);

    unsigned char getRights() const;
    
private:
    std::bitset<4> mCastleRightsBits;
};