#pragma once
#include <cstdint>
#include "Vector2i.hpp"
#include "castleRights.hpp"

struct ChessMove
{
    enum struct MoveTypes : uint8_t
    {
        INVALID = 0,
        NORMAL,
        DOUBLE_PUSH,
        ENPASSANT,
        CASTLE,
        PROMOTION,
    };

    //The types of pieces you can promote a pawn to.
    enum struct PromoTypes : uint8_t
    {
        INVALID = 0, //INVALID signifies that no promotion happened on this move
        QUEEN,
        ROOK,
        KNIGHT,
        BISHOP
    };

    Vec2i      src         {INVALID_VEC2I};  //where the piece is moving to
    Vec2i      dest        {INVALID_VEC2I};  //where the piece moved from
    PromoTypes promoType   {PromoTypes::INVALID};
    MoveTypes  moveType    {MoveTypes::INVALID};
    CastleRights rightsToRevoke {};
    bool wasOpponentsMove {false}; //ignored if playing offline
    bool wasCapture {false};
    
    ChessMove()=default;

    ChessMove(Vec2i src_, Vec2i dest_, bool wasCapture_,
        MoveTypes moveType_ = MoveTypes::NORMAL,
        CastleRights rightsToRevoke_ = {}, 
        PromoTypes promoType_ = PromoTypes::INVALID) :  
        src{src_}, dest{dest_}, wasCapture{wasCapture_},
        moveType{moveType_}, promoType{promoType_},
        rightsToRevoke{rightsToRevoke_}
    {}

    //the defaulted c++20 spaceship operator allows compiler 
    //to supply default comparison operators for ChessMove
    auto operator<=>(ChessMove const&) const = default;
};