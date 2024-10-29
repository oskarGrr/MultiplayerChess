#pragma once
#include "Vector2i.h"
#include "chessNetworkProtocol.h" //enum Side
#include "SDL.h"
#include <string>
#include <array>
#include <vector>
#include <memory> //std::shared_ptr
#include <optional>
#include <unordered_map>

#include "ChessEvents.hpp"
#include "ChessMove.hpp"
#include "castleRights.h"

class Piece;
class ConnectionManager;
class SoundManager;

class Board
{
public:

    Board(BoardEventSystem::Publisher const&, GUIEventSystem::Subscriber&, NetworkEventSystem::Subscriber&);

    //factory method for placing a piece at the specified location on the board
    //these should only be called in the boards constructor ideally to follow RAII.
    //except for when there is a call to either ConreteTy = Queen, Rook, Knight, or Bishop upon promotion.
    //all of the memory will be freed in the boards destructor
    template<typename ConcreteTy> 
    void makeNewPieceAt(Vec2i const& pos, Side side);

    void pickUpPiece(Vec2i chessPos) const;
    void putPieceDown(Vec2i chessPos);

    void resetBoard();
    bool hasCastleRights(CastleRights) const;
    
    static bool isValidChessPosition(Vec2i);

    //supply with one of the enums above and will respond with true or false
    void removeCastlingRights(CastleRights);
    std::shared_ptr<Piece> getPieceAt(Vec2i const& chessPos) const&;

    Side getWhosTurnItIs() const {return mWhiteOrBlacksTurn;}
    std::vector<Vec2i> getAttackedSquares(Side) const; //Get all the squares that are under attack for a given side.
    
    void setLastCapturedPiece(auto p) {m_lastCapturedPiece = p;}
    void setSideUserIsPlayingAs(Side s) {m_sideUserIsPlayingAs = s;}
    auto getSideUserIsPlayingAs() const {return m_sideUserIsPlayingAs;}
    auto const& getPieces() const {return m_pieces;}

    enum struct CheckType{INVALID = -1, NO_CHECK, SINGLE_CHECK, DOUBLE_CHECK};
    CheckType getCheckState() const {return mCurrentCheckType;}
    Vec2i getLocationOfCheckingPiece() const {return mCheckingPieceLocation;} //Get the chess position of the piece which is putting a king in check.

    void updateEnPassant(Vec2i newPostition);
    auto isEnPassantAvailable() const {return mEnPassantLocation != INVALID_VEC2I;}
    auto getEnPassantLocation() const {return mEnPassantLocation;}
    void resetEnPassant() {mEnPassantLocation = INVALID_VEC2I;}

    void updateLegalMoves();//updates the pieces internal psuedo legal, fully legal, and attacked squares vectors
    void updatePinnedPieces();
    void updatePseudoLegalsAndAttackedSquares();//update the pieces _PseudoLegals and get a vector of all attacked squares for the side that just made a move
    void updateCheckState();//update m_checkState

    void toggleTurn();//change who's turn it is to move 

private:

    BoardEventSystem::Publisher const& mBoardEventPublisher;

    enum struct SubscriptionTypes
    {
        RESET_BOARD,
        PROMOTION_END,
        PAIRING_COMPLETE,
        OPPONENT_MADE_MOVE,
        UNPAIRED
    };

    SubscriptionManager<SubscriptionTypes,
        GUIEventSystem::Subscriber> mGuiSubManager;

    SubscriptionManager<SubscriptionTypes,
        NetworkEventSystem::Subscriber> mNetworkSubManager;

    std::array<std::shared_ptr<Piece>, 64> m_pieces {};
    std::shared_ptr<Piece> m_lastCapturedPiece {nullptr};

    CheckType mCurrentCheckType {CheckType::INVALID}; //If there is no check currently, this will be set to NO_CHECK
    Vec2i     mCheckingPieceLocation {INVALID_VEC2I}; //where is the piece putting a king in check otherwise INVALID_CHESS_SQUARE
    Vec2i     m_locationOfSecondCheckingPiece {INVALID_VEC2I}; //if the check state is in double check where is the second piece putting the king in check

    Side mWhiteOrBlacksTurn {Side::WHITE};
    Side m_sideUserIsPlayingAs {Side::INVALID}; //only used when playing against an opponent online

    CastleRights m_castlingRights {CastleRights::NONE};

    //The position of the en passant square is (where the pawn will land after capturing).
    //INVALID_CHESS_SQUARE if there is no en passant available.
    Vec2i mEnPassantLocation {INVALID_VEC2I};

    ChessMove mLastMoveMade;

    //methods called inside of postMoveUpdate() for handling certain special move types
    void handleDoublePushMove();
    void handleCastleMove();
    void handleEnPassantMove();
    void handleRookMove();
    void handleRookCapture();
    void handleKingMove();

    void postMoveUpdate();
    void movePiece(ChessMove const& move);
    void capturePiece(Vec2i location);

    //called from piecePutDownRoutine() to see if the move being requested
    //is in the vector of fully legal moves for the piece on the mouse
    auto requestMove(Vec2i const& destinationSquare);

    //loads up a fen string into the board. 
    //makes some assumptions that the given string is a valid fen string.
    //in the future I will probably make a seperate class for loading the 
    //different portions of a fen string in order to break up this method which is a little lengthy
    void loadFENIntoBoard(std::string_view fenString);

    enum struct MateTypes {INVALID, CHECKMATE, STALEMATE};

    //std::nullopt means no check/stalemate has occurred.
    std::optional<MateTypes> hasCheckOrStalemateOccurred();

    //Helper function to reduce constructor size.
    void subToEvents();

public:
    Board(Board const&)=delete;
    Board(Board&&)=delete;
    Board& operator=(Board const&)=delete;
    Board& operator=(Board&&)=delete;
};