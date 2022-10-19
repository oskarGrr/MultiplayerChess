#pragma once
#include "SDL.h"
#include "Vector2i.h"
#include "ChessApplication.h"
#include <vector>
#include <array>

class Board;

#define NUM_OF_PIECE_TEXTURES 12 //6 types of pieces * 2 for both sides

enum struct Side : Uint32
{
    BLACK, WHITE
};

//a signal to be stored with moves that the board 
//listens to after a move is made
enum struct MoveInfo : Uint32
{
    NORMAL,
    DOUBLE_PUSH,   //a double pawn push move
    ENPASSANT,     //an en passant capture move
    PROMOTION,     //a pawn promotion move
    CASTLE,        //a castling move
    ROOK_MOVE,
    KING_MOVE,
    ROOK_CAPTURE,
    ROOK_CAPTURE_AND_PROMOTION //case where a pawn catures a rook and promotes
};

//a Move type is now a Vector 2 that holds where the piece 
//is moving to and a MoveInfo enum that holds a signal to be 
//consumed by the board after that move is made
using Move = std::pair<Vec2i, MoveInfo>;

enum struct textureIndices : Uint32
{
    WPAWN, WKNIGHT, WROOK, WBISHOP, WQUEEN, WKING,
    BPAWN, BKNIGHT, BROOK, BBISHOP, BQUEEN, BKING
};

//this class isnt responsible for ownership
//of the Pieces. the ChessApp::_Board is, because the board "holds" the pieces.
//This abstract Piece class is meant to serve as a base class for
//the concrete pieces to derive from, and isnt meant to be instantiated by itself
class Piece
{
public:

    Piece()=delete;                         //redundant to delete these since you
    Piece(Piece const&)=delete;             //cant instantiate this class anyway (its abstract)
    Piece(Piece&&)=delete;                  //but I like to put these in every class
    Piece& operator=(Piece const&)=delete;  
    Piece& operator=(Piece&&)=delete;

    Piece(Side const side, Vec2i const chessPos);
    virtual ~Piece();
    
protected:

    inline static std::array<SDL_Texture*, NUM_OF_PIECE_TEXTURES> s_textures{};//gets initialized when pieces are made
    inline static constexpr float s_scale{0.32f};//how much to scale down the textures
    inline static std::shared_ptr<Piece> s_pieceOnMouse{nullptr};//the piece the mouse is holding otherwise nullptr

    std::vector<Move> m_pseudoLegals; //all of the pseudo legal moves a piece has and their types
    std::vector<Move> m_legalMoves;   //all of the fully legal moves a piece has and their types

    Side const m_side;                          //black or white piece
    Vec2i m_chessPos;                           //the file and rank (x,y) of where the piece is (0-7)
    Vec2i m_screenPos;                          //location (on the screen) of the middle of the square that the piece is on
    std::vector<Vec2i> m_attackedSquares;       //all the squares being attacked by *this
    SDL_Texture* m_texture;                     //every piece will have an SDL texture that will point to the correct piece texture
    SDL_Rect m_sourceRect;
    SDL_Rect m_destRect;

public:

    virtual void updatePseudoLegalAndAttacked()=0;//updates a piece's m_pseudoLegals and m_attackedSquares   
    virtual void updateLegalMoves()=0;//updates the full legal moves for a given concrete piece

    void drawPieceOnMouse() const;
    void draw() const;
    void updatePinnedInfo();//updates a pieces m_locationOfPiecePinningThis
    void setScreenPos(Vec2i const newPos);
    inline Vec2i getScreenPos() const {return m_screenPos;}
    inline bool isPiecePinned() const {return m_locationOfPiecePinningThis != Vec2i{-1, -1};};//if m_locationOfPiecePinningThis is == -1, -1 then there isnt a piece pinning *this to its king
    inline static auto getPieceOnMouse(){return s_pieceOnMouse;}
    inline static void setPieceOnMouse(std::shared_ptr<Piece> const& updateTo = nullptr){ s_pieceOnMouse = updateTo; }
    static std::array<SDL_Texture*, NUM_OF_PIECE_TEXTURES> const& getPieceTextures();
    inline static constexpr float getPieceTextureScale(){return s_scale;}
    void setChessPosition(Vec2i const setChessPosition);
    inline Vec2i getChessPosition() const {return m_chessPos;}
    inline Side getSide() const {return m_side;}
    inline std::vector<Move> const& getPseudoLegalMoves() const {return m_pseudoLegals;}
    inline std::vector<Move> const& getLegalMoves() const {return m_legalMoves;}
    inline std::vector<Vec2i> const& getAttackedSquares() const {return m_attackedSquares;}

private:

    static void initTextures();//called once in the base ctor
    static void destoryTextures();//called once in the dtor
    inline void resetLocationOfPiecePinningThis(){m_locationOfPiecePinningThis = {-1,-1};}

protected:

    enum struct PieceType : Uint32 {INVALID = 0, PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING};
    PieceType m_type;//the type of the concrete piece extending this abstract class

    //here so pieces can see the m_type of other pieces instead of using dynamic_cast
    inline static PieceType getType(Piece const& other) {return other.m_type;}

    Vec2i m_locationOfPiecePinningThis;//the location of the piece (if there is one otherwise -1, -1) pinning *this to its king

    //used by queens and rooks to calculate their legal moves
    //changes _psuedoLegals and pushes the squares being attacked by *this into the vector passed in
    void orthogonalSlide();

    //same thing as orthogonal slide except used by queens and bishops
    void diagonalSlide();

    //this function assumes Board::m_checkState is equal to SINGLE_CHECK.
    //called by the concrete implementations of virtual void updateLegalMoves()=0;
    static bool doesNonKingMoveResolveCheck(Move const&, Vec2i const posOfCheckingPiece);

    void initRects();//just called in the derived ctors to set up the texture sizes
    static bool areSquaresOnSameDiagonal(Vec2i const, Vec2i const);
    static bool areSquaresOnSameRankOrFile(Vec2i const, Vec2i const);
};

class Pawn : public Piece
{
public:

    Pawn(Side const side, Vec2i const chessPos);
    Pawn()=delete;
    Pawn(Pawn const&)=delete;
    Pawn(Pawn&&)=delete;

private:

    void updatePseudoLegalAndAttacked() override;
    void updateLegalMoves() override;

    //called by updateLegalMoves to solve an edge case where an 
    //en passant capture would put *this' king into check
    bool doesEnPassantLeaveKingInCheck(Vec2i const enPassantMove) const;
};

class Knight : public Piece
{
public:

    Knight(Side const side, Vec2i const chessPos);
    Knight()=delete;
    Knight(Knight const&)=delete;
    Knight(Knight&&)=delete;

private:

    void updatePseudoLegalAndAttacked() override;
    void updateLegalMoves() override;
};

class Rook : public Piece
{
public:

    Rook(Side const side, Vec2i const chessPos);
    Rook()=delete;
    Rook(Rook const&)=delete;
    Rook(Rook&&)=delete;

    enum struct KingOrQueenSide{NEITHER, QUEEN_SIDE, KING_SIDE};
    inline void setKingOrQueenSide(KingOrQueenSide const koqs){m_koqs = koqs;}//change m_koqs
    inline KingOrQueenSide getKingOrQueenSide() const {return m_koqs;}
    inline void setIfRookHasMoved(bool const hasMoved){m_hasMoved = hasMoved;}//set m_hasMoved
    inline bool getHasMoved() const {return m_hasMoved;}

private:

    bool m_hasMoved;

    //will be set to NEITHER when FEN string is loaded.
    //will only be set to queen or king side if the FEN string
    //has castling rights for the given rook.
    KingOrQueenSide m_koqs;

    void updatePseudoLegalAndAttacked() override;
    void updateLegalMoves() override;
};

class Bishop : public Piece
{
public:

    Bishop(Side const side, Vec2i const chessPos);
    Bishop()=delete;
    Bishop(Bishop const&)=delete;
    Bishop(Bishop&&)=delete;   

private:

    void updatePseudoLegalAndAttacked() override;
    void updateLegalMoves() override;
};

class Queen : public Piece
{
public:

    Queen(Side const side, Vec2i const chessPos);
    Queen()=delete;
    Queen(Queen const&)=delete;
    Queen(Queen&&)=delete;

private:

    void updatePseudoLegalAndAttacked() override;
    void updateLegalMoves() override;
};

class King : public Piece
{
public:

    King(Side const side, Vec2i const chessPos);
    King()=delete;
    King(King const&)=delete;
    King(King&&)=delete;

private:

    void updatePseudoLegalAndAttacked() override;
    void updateLegalMoves() override;

    //remeber where the kings are for easy lookup
    inline static Vec2i s_wKingPos{};
    inline static Vec2i s_bKingPos{};

public:

    inline static Vec2i getWhiteKingPos(){return s_wKingPos;}
    inline static Vec2i getBlackKingPos(){return s_bKingPos;}
    inline static void setWhiteKingPos(Vec2i const pos){s_wKingPos = pos;}
    inline static void setBlackKingPos(Vec2i const pos){s_bKingPos = pos;}
};