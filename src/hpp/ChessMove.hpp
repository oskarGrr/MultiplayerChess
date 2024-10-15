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
        NORMAL_CAPTURE, //a capture that inst an en passant or rook capture
        DOUBLE_PUSH,    //a double pawn push
        ENPASSANT,      //an en passant capture
        PROMOTION,      //a pawn promotion
        CASTLE,         //a castling move
        ROOK_MOVE,
        KING_MOVE,
        ROOK_CAPTURE,
        ROOK_CAPTURE_AND_PROMOTION //case where a pawn catures a rook and promotes
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
    MoveTypes  moveType    {MoveTypes::INVALID};
    PromoTypes promoType   {PromoTypes::INVALID};
    bool       wasACapture {false}; //did this move capture an enemy piece

    ChessMove()=default;

    ChessMove(Vec2i src, Vec2i dest, MoveTypes mType,
        bool wasCapture, PromoTypes pType = PromoTypes::INVALID) :
        src{src}, dest{dest}, moveType{mType}, promoType{pType} {}

    //Defaulted c++20 spaceship operator allows compiler 
    //to supply default comparison operators for this struct.
    auto operator<=>(ChessMove const&) const = default;
};