#pragma once
#include <cstdint>
#include "Vector2i.h"

//this just holds some inline stuff and enums for 
//the move type and related information

enum struct MoveInfo : uint32_t
{
    INVALID = 0,
    NORMAL,
    NORMAL_CAPTURE, //a capture that inst an en passant or rook capture
    DOUBLE_PUSH,    //a double pawn push move
    ENPASSANT,      //an en passant capture move
    PROMOTION,      //a pawn promotion move
    CASTLE,         //a castling move
    ROOK_MOVE,
    KING_MOVE,
    ROOK_CAPTURE,
    ROOK_CAPTURE_AND_PROMOTION //case where a pawn catures a rook and promotes
};

//a move is a vec2 of where the piece moves to, where it moved from, and the type of move
struct Move
{
    Vec2i    m_source; //where the piece is moving to
    Vec2i    m_dest;   //where the piece moved from
    MoveInfo m_moveType; //the type of the move
    inline Move() : m_source{-1, -1}, m_dest{-1, -1}, m_moveType{MoveInfo::INVALID} {}
    inline Move(Vec2i const& source, Vec2i const& dest, MoveInfo const& moveType) :
        m_source{source}, m_dest{dest}, m_moveType{moveType} {}
    inline bool operator==(Move const& rhs) const 
    {
        return m_source == rhs.m_source && m_dest == rhs.m_dest && m_moveType == rhs.m_moveType;
    }
};

//the types of pieces you can promote a pawn to
enum struct PromoType : uint32_t
{
    INVALID = 0, //invalid means no promotion
    QUEEN_PROMOTION,
    ROOK_PROMOTION,
    KNIGHT_PROMOTION,
    BISHOP_PROMOTION
};