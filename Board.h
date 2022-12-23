#pragma once
#include "Vector2i.h"
#include "SDL.h"
#include <string>
#include <array>
#include <vector>
#include <memory>//std::shared_ptr

class Piece;

enum struct Side : Uint32;

//a signal to be stored with moves that the board 
//listens to after a move is made
enum struct MoveInfo : Uint32
{
    NORMAL,
    NORMAL_CAPTURE, //a capture that inst an en passant or rook capture
    DOUBLE_PUSH,    //a double pawn push move
    ENPASSANT,      //an en passant capture move
    PROMOTION,      //a pawn promotion move
    CASTLE,         //a castling move
    ROOK_MOVE,
    KING_MOVE,
    ROOK_CAPTURE,
    ROOK_CAPTURE_AND_PROMOTION, //case where a pawn catures a rook and promotes
};

enum struct WinLossDrawTypes : Uint32
{
    INVALID, //if the sate of m_winLossDrawType is INVALID the game is still in progress
    CHECKMATE,
    STALEMATE,
    GAME_ABANDONMENT, //win from the other player abandoning game
    DRAW_AGREEMENT
};

using Move = std::pair<Vec2i, MoveInfo>;

//bit masks to indicate what castling rights are available.
enum CastleRights : Uint32
{
    NONE = 0, WSHORT = 0b1, WLONG = 0b10, BSHORT = 0b100, BLONG = 0b1000
};

inline CastleRights operator|(CastleRights const lhs, CastleRights const rhs)
{
    return static_cast<CastleRights>(static_cast<Uint32>(lhs) | 
        static_cast<Uint32>(rhs));
}

inline CastleRights& operator|=(CastleRights& lhs, CastleRights const rhs)
{
    return lhs = static_cast<CastleRights>(lhs | rhs);
}

inline CastleRights operator&(CastleRights const lhs, CastleRights const rhs)
{
    return static_cast<CastleRights>(static_cast<Uint32>(lhs) &
        static_cast<Uint32>(rhs));
}

inline CastleRights& operator&=(CastleRights& lhs, CastleRights const rhs)
{
    return lhs = static_cast<CastleRights>(lhs & rhs);
}

//Board object only instantiated as a member of the chessApplication.
class Board
{
public:

    Board();
    ~Board();
    Board(Board const&)=delete;
    Board(Board&&)=delete;
    Board& operator=(Board const&)=delete;
    Board& operator=(Board&&)=delete;   

    //factory method for placing a piece at the specified location on the board
    //these should only be called in the boards constructor ideally to follow RAII.
    //except for when there is a call to either ConreteTy = Queen, Rook, Knight, or Bishop upon promotion.
    //all of the memory will be freed in the boards destructor
    template<typename ConcreteTy> 
    void makeNewPieceAt(Vec2i const& pos, Side const side);

    void resetBoard();//reset to starting position
    bool hasCastleRights(const CastleRights cr) const;//supply with one of the enums above and will respond with true or false
    void removeCastlingRights(const CastleRights cr);
    void piecePickUpRoutine(SDL_Event const&) const;
    void piecePutDownRoutine(SDL_Event const&);
    std::shared_ptr<Piece> getPieceAt(Vec2i const& chessPos) const;
    void updateEnPassant(Vec2i const& newPostition);
    inline bool isThereEnPassantAvailable() const {return m_enPassantPosition != Vec2i{-1, -1};}
    inline Vec2i const& getEnPassantIndex() const {return m_enPassantPosition;}
    inline void resetEnPassant(){m_enPassantPosition = {-1, -1};}//reset the enPassantTargetLocation back to -1, -1
    inline Side getWhosTurnItIs() const {return m_sideOfWhosTurnItIs;}
    std::vector<Vec2i> getAttackedSquares(Side isWhite) const;//get all the attacked squares for the given side 
    inline Side getViewingPerspective() const {return m_viewingPerspective;}//get which side the user is viewing the board from
    void capturePiece(Vec2i const& location);//moves a Piece* to the captured piece pool
    inline auto getLastCapturedPiece(){return m_lastCapturedPiece;}
    inline void setLastCapturedPiece(auto p){m_lastCapturedPiece = p;}
    inline void setSideUserIsPlayingAs(Side s){m_sideUserIsPlayingAs = s;}
    inline Side getSideUserIsPlayingAs() const {return m_sideUserIsPlayingAs;}
    inline auto& getPieces() {return m_pieces;}

    enum struct CheckState{INVALID = 0, NO_CHECK, SINGLE_CHECK, DOUBLE_CHECK};
    inline CheckState getCheckState() const {return m_checkState;}
    inline Vec2i const& getLocationOfCheckingPiece() const {return m_locationOfCheckingPiece;}
    inline Vec2i const& getLocationOfSecondCheckingPiece() const {return m_locationOfSecondCheckingPiece;}

    void updateLegalMoves();//updates the pieces internal psuedo legal, fully legal, and attacked squares vectors
    void updatePinnedPieces();
    void updatePseudoLegalsAndAttackedSquares();//update the pieces _PseudoLegals and get a vector of all attacked squares for the side that just made a move
    void updateCheckState();//update m_checkState
    void movePiece(Vec2i const& source, Vec2i const& destination);//all piece moves should go through this method
    void loadFENIntoBoard(std::string const&);

    //methods called inside of postMoveUpdate() for handling certain special move types
    void handleDoublePushMove(Vec2i const& doublePushPosition);
    void handleCastleMove(Vec2i const& castleSquare);
    void handleEnPassantMove();
    void handleRookMove(Vec2i const& newRookPos);
    void handleRookCapture();
    void handleKingMove(Vec2i const& newKingPos);

    //to be called in postMoveUpdate() after the correct above single hanlde 
    //method was called and before m_lastCapturedPiece is reset to null
    void playCorrectMoveAudio(Move const&);

    auto requestMove(Vec2i const& requestedPosition);
    void commitMove(Move const& newMove);
    void postMoveUpdate(Move const& newMove, Vec2i const& postMoveUpdate);

    void toggleTurn();//change whos turn it is to move
    void flipBoardViewingPerspective();
    inline void setBoardViewingPerspective(Side s) {m_viewingPerspective = s;}

private:
    
    std::array<std::shared_ptr<Piece>, 64> m_pieces;
    std::shared_ptr<Piece> m_lastCapturedPiece;

    CheckState       m_checkState;   
    Vec2i            m_locationOfCheckingPiece;       //where is the piece putting a king in check otherwise -1, -1
    Vec2i            m_locationOfSecondCheckingPiece; //if the check state is in double check where is the second piece putting the king in check
    Vec2i            m_enPassantPosition;             //position of the square where en passant is possible in the next move
    Side             m_sideOfWhosTurnItIs;            //used to remember whos turn is it
    CastleRights     m_castlingRights;                //bitwise & with the CastleRights enumerations
    Side             m_viewingPerspective;            //the side (white or black) that the player is viewing the board from
    Side             m_sideUserIsPlayingAs;           //the side the user is playing (only used when connected to another player)
    WinLossDrawTypes m_winLossDrawState;              //the current state of the game in terms of if the game is a win/loss/draw (INVALID means the game is ongoing)
};