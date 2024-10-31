#pragma once
#include <cstdint>
#include "Vector2i.h"

//a move is a vec2 of where the piece moves to, where it moved from, and the type of move
struct ChessMove
{
    enum struct MoveTypes : uint8_t
    {
        INVALID = 0,
        NORMAL,
        NORMAL_CAPTURE,
        DOUBLE_PUSH, 
        ENPASSANT,
        CASTLE,
        ROOK_MOVE,
        KING_MOVE,
        ROOK_CAPTURE,
        PROMOTION,      
        PROMOTION_CAPTURE,
        PROMOTION_ROOK_CAPTURE
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

    bool       wasOpponentsMove {false}; //ignored if playing offline
    
    ChessMove()=default;

    ChessMove(Vec2i src, Vec2i dest, MoveTypes mType,
        PromoTypes pType = PromoTypes::INVALID, bool wasOpponentsMove_ = false) :
        src{src}, dest{dest},
        moveType{mType}, promoType{pType},
        wasOpponentsMove{wasOpponentsMove_} {}

    //Defaulted c++20 spaceship operator allows compiler 
    //to supply default comparison operators for this struct.
    auto operator<=>(ChessMove const&) const = default;

    bool wasMoveACapture() const
    {
        return moveType == MoveTypes::NORMAL_CAPTURE ||
               moveType == MoveTypes::ROOK_CAPTURE ||
               moveType == MoveTypes::PROMOTION_ROOK_CAPTURE ||
               moveType == MoveTypes::ENPASSANT ||
               moveType == MoveTypes::PROMOTION_CAPTURE;
    }
};