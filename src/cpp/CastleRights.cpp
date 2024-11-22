#include "castleRights.h"
#include <cassert>

CastleRights::CastleRights(unsigned char rights) : mCastleRightsBits{rights} {}

bool CastleRights::hasRights(Rights rights) const
{
    return mCastleRightsBits.test(static_cast<size_t>(rights));
}

bool CastleRights::hasRights(Side whiteOrBlack) const
{
    if(whiteOrBlack == Side::WHITE)
    {
        return mCastleRightsBits.test(static_cast<size_t>(Rights::WLONG)) &&
               mCastleRightsBits.test(static_cast<size_t>(Rights::WSHORT));
    }
    else
    {
        return mCastleRightsBits.test(static_cast<size_t>(Rights::BLONG)) &&
               mCastleRightsBits.test(static_cast<size_t>(Rights::BSHORT));
    }
}

void CastleRights::revokeRights(Rights rights)
{
    mCastleRightsBits.set(static_cast<size_t>(rights), false);
}

void CastleRights::revokeRights(Side whiteOrBlack)
{
    if(whiteOrBlack == Side::WHITE)
    {
        mCastleRightsBits.set(static_cast<size_t>(Rights::WLONG), false);
        mCastleRightsBits.set(static_cast<size_t>(Rights::WSHORT), false);
    }
    else
    {
        mCastleRightsBits.set(static_cast<size_t>(Rights::BLONG), false);
        mCastleRightsBits.set(static_cast<size_t>(Rights::BSHORT), false);
    }
}

void CastleRights::revokeRights(CastleRights const& rightsToRevoke)
{
    mCastleRightsBits &= ~rightsToRevoke.mCastleRightsBits;
}

void CastleRights::addRights(Rights rights)
{
    mCastleRightsBits.set(static_cast<size_t>(rights));
}

void CastleRights::addRights(Side whiteOrBlack)
{
    if(whiteOrBlack == Side::WHITE)
    {
        mCastleRightsBits.set(static_cast<size_t>(Rights::WLONG));
        mCastleRightsBits.set(static_cast<size_t>(Rights::WSHORT));
    }
    else
    {
        mCastleRightsBits.set(static_cast<size_t>(Rights::BLONG));
        mCastleRightsBits.set(static_cast<size_t>(Rights::BSHORT));
    }
}

void CastleRights::addRights(CastleRights const& rightsToAdd)
{
    mCastleRightsBits |= rightsToAdd.mCastleRightsBits;
}

unsigned char CastleRights::getRights() const
{
    return mCastleRightsBits.to_ulong();
}