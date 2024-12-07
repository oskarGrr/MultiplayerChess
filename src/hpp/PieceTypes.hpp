#pragma once
#include "SDL.h"
#include "Vector2i.hpp"
#include "chessNetworkProtocol.h" //enum Side
#include "TextureManager.hpp" //enum TextureManager::WhichTexture
#include "ChessMove.hpp"
#include <vector>
#include <array>
#include <type_traits>

class Board;

//this class isnt responsible for ownership
//of the Pieces. the ChessApp::_Board is, because the board "holds" the pieces.
//This abstract Piece class is meant to serve as a base class for
//the concrete pieces to derive from, and isnt meant to be instantiated by itself
class Piece
{
public:
    Piece(Side side, Vec2i chessPos);
    virtual ~Piece()=default;
    
protected:
    inline static std::shared_ptr<Piece> s_pieceOnMouse{nullptr};//the piece the mouse is holding otherwise nullptr
    
    std::vector<ChessMove> m_pseudoLegals; //all of the pseudo legal moves a piece has and their types
    std::vector<ChessMove> m_legalMoves;   //all of the fully legal moves a piece has and their types

    Side const m_side;                       //black or white piece
    Vec2i m_chessPos;                        //the file and rank (x,y) of where the piece is (0-7)
    std::vector<Vec2i> m_attackedSquares;    //all the squares being attacked by *this
    TextureManager::WhichTexture m_whichTexture; //array offset into the array of piece textures owned by ChessApp signifying which texture belongs to this piece              

public:
    virtual void updatePseudoLegalAndAttacked(Board const& b)=0;//updates a piece's m_pseudoLegals and m_attackedSquares   

    //updates the fullly legal moves for a given concrete piece.
    //after the pieces pseudo legal moves, pinned pieces, and the board check 
    //state have been updated (and the opposite sides attacked squares)
    //then this method should be used.
    virtual void updateLegalMoves(Board const& b)=0;

    void updatePinnedInfo(Board const& b);//updates a pieces m_locationOfPiecePinningThis
    bool isPiecePinned() const {return m_locationOfPiecePinningThis != INVALID_VEC2I;};//if m_locationOfPiecePinningThis is == INVALID_VEC2I then there isnt a piece pinning *this to its king
    void setChessPosition(Vec2i setChessPosition);
    Vec2i getChessPosition() const {return m_chessPos;}
    Side getSide() const {return m_side;}
    std::vector<ChessMove> const& getPseudoLegalMoves() const {return m_pseudoLegals;}
    std::vector<ChessMove> const& getLegalMoves() const {return m_legalMoves;}
    std::vector<Vec2i> const& getAttackedSquares() const {return m_attackedSquares;}
    auto getWhichTexture() const {return m_whichTexture;}

    static auto getPieceOnMouse(){return s_pieceOnMouse;}
    static void setPieceOnMouse(std::shared_ptr<Piece> const& updateTo = nullptr){s_pieceOnMouse = updateTo;}
    static void resetPieceOnMouse(){s_pieceOnMouse.reset();}

private:
    void resetLocationOfPiecePinningThis(){m_locationOfPiecePinningThis = INVALID_VEC2I;}

protected:
    enum struct PieceTypes : uint32_t {INVALID = 0, PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING};
    PieceTypes m_type;//the type of the concrete piece extending this abstract class

    //here so pieces can see the m_type of other pieces instead of doing a down cast
    static PieceTypes getType(Piece const& other) {return other.m_type;}

    Vec2i m_locationOfPiecePinningThis;//the location of the piece (if there is one otherwise INVALID_VEC2I) pinning *this to its king

    void orthogonalSlide(Board const& b);
    void diagonalSlide(Board const& b);

    //this function assumes Board::m_checkState is equal to SINGLE_CHECK.
    //called by the concrete implementations of virtual void updateLegalMoves()=0;
    static bool doesNonKingMoveResolveCheck(ChessMove const&, Vec2i posOfCheckingPiece, Board const& b);

    static bool areSquaresOnSameDiagonal(Vec2i, Vec2i);
    static bool areSquaresOnSameRankOrFile(Vec2i, Vec2i);
};

class Pawn : public Piece
{
public:
    Pawn(Side side, Vec2i chessPos);

private:
    void updatePseudoLegalAndAttacked(Board const& b) override;
    void updateLegalMoves(Board const& b) override;

    //called by updateLegalMoves to solve an edge case where an 
    //en passant capture would put *this' king into check
    bool doesEnPassantLeaveKingInCheck(Vec2i enPassantMove, Board const& b) const;
};

class Knight : public Piece
{
public:
    Knight(Side side, Vec2i chessPos);

private:
    void updatePseudoLegalAndAttacked(Board const& b) override;
    void updateLegalMoves(Board const& b) override;
};

class Rook : public Piece
{
public:
    Rook(Side side, Vec2i chessPos);

    enum struct KingOrQueenSide{NEITHER, QUEEN_SIDE, KING_SIDE};
    void setKingOrQueenSide(KingOrQueenSide koqs){m_koqs = koqs;}
    KingOrQueenSide getKingOrQueenSide() const {return m_koqs;}

private:

    //did this rook start as a queen's rook or a king's rook
    KingOrQueenSide m_koqs;

    void updatePseudoLegalAndAttacked(Board const& b) override;
    void updateLegalMoves(Board const& b) override;
};

class Bishop : public Piece
{
public:
    Bishop(Side side, Vec2i chessPos);

private:
    void updatePseudoLegalAndAttacked(Board const& b) override;
    void updateLegalMoves(Board const& b) override;
};

class Queen : public Piece
{
public:
    Queen(Side side, Vec2i chessPos);

private:

    void updatePseudoLegalAndAttacked(Board const& b) override;
    void updateLegalMoves(Board const& b) override;
};

class King : public Piece
{
public:
    King(Side side, Vec2i chessPos);

private:
    void updatePseudoLegalAndAttacked(Board const& b) override;
    void updateLegalMoves(Board const& b) override;

    //remeber where the kings are for easy lookup
    inline static Vec2i s_wKingPos{};
    inline static Vec2i s_bKingPos{};

public:
    static Vec2i getWhiteKingPos(){return s_wKingPos;}
    static Vec2i getBlackKingPos(){return s_bKingPos;}
    static void setWhiteKingPos(Vec2i pos){s_wKingPos = pos;}
    static void setBlackKingPos(Vec2i pos){s_bKingPos = pos;}
};