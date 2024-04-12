#pragma once
#include "Vector2i.h"
#include "SDL.h"
#include <string>
#include <array>
#include <vector>
#include <memory>//for std::shared_ptr

#include "chessAppLevelProtocol.h"
#include "moveType.h"
#include "castleRights.h"

class Piece;
enum struct GameState : uint32_t;

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

    //the two main methods called inside ChessApp::processEvents()
    void piecePickUpRoutine(SDL_Event const&) const;
    void piecePutDownRoutine(SDL_Event const&);

    void resetBoard();
    bool hasCastleRights(const CastleRights cr) const;//supply with one of the enums above and will respond with true or false
    void removeCastlingRights(const CastleRights cr);
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
    inline auto const& getPieces() const {return m_pieces;}
    inline Move const& getLastMove() const {return m_lastMoveMade;}

    enum struct CheckState{INVALID = 0, NO_CHECK, SINGLE_CHECK, DOUBLE_CHECK};
    inline CheckState getCheckState() const {return m_checkState;}
    inline Vec2i const& getLocationOfCheckingPiece() const {return m_locationOfCheckingPiece;}
    inline Vec2i const& getLocationOfSecondCheckingPiece() const {return m_locationOfSecondCheckingPiece;}

    void updateLegalMoves();//updates the pieces internal psuedo legal, fully legal, and attacked squares vectors
    void updatePinnedPieces();
    void updatePseudoLegalsAndAttackedSquares();//update the pieces _PseudoLegals and get a vector of all attacked squares for the side that just made a move
    void updateCheckState();//update m_checkState
    void movePiece(Move const& move);//all piece moves should go through this method

    //to be called in postMoveUpdate() after the correct above single hanlde 
    //method was called and before m_lastCapturedPiece is reset to null
    void playCorrectMoveAudio(MoveInfo);

    void toggleTurn();//change whos turn it is to move

    //whichPromoType is defaulted to PromoType::INVALID to indicate no promotion. otherwise
    //whichPromoType will indicate the type of piece being promoted to
    void postMoveUpdate(Move const& newMove, PromoType whichPromoType = PromoType::INVALID);

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
    Move             m_lastMoveMade;                  //the last move that was made (is updated after movePiece() not postMoveUpdate())

    //methods called inside of postMoveUpdate() for handling certain special move types
    void handleDoublePushMove(Vec2i const& doublePushPosition);
    void handleCastleMove(Move const& castleMove);
    void handleEnPassantMove();
    void handleRookMove(Vec2i const& newRookPos);
    void handleRookCapture();
    void handleKingMove(Vec2i const& newKingPos);

    //called from piecePutDownRoutine() to see if the move being requested
    //is in the vector of fully legal moves for the piece on the mouse
    auto requestMove(Vec2i const& destinationSquare);

    //loads up a fen string into the board. 
    //makes some assumptions that the given string is a valid fen string.
    //in the future I will probably make a seperate class for loading the 
    //different portions of a fen string in order to break up this method which is a little lengthy
    void loadFENIntoBoard(std::string const&);

    //will check for stale or check mate. if either one is true then update the game state to that.
    void checkForCheckOrStaleM8(Side const sideToCheck);
};