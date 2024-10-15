#include "Board.hpp"
#include "SDL.h"
#include "PieceTypes.h"
#include "ChessEvents.hpp"
#include "errorLogger.h"
#include "SoundManager.hpp"

#include <string>
#include <exception>
#include <fstream>
#include <cassert>

static constexpr auto startingPosFEN {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
//static constexpr auto stalemateTestPositionFEN {"7k/8/8/8/8/8/6q1/K7 w - 0 1"};

static int chessPos2Index(Vec2i const pos)
{
    return pos.y * 8 + pos.x;
}

static Vec2i index2ChessPos(int const index)
{
    return {index % 8, index / 8};
}

Board::Board(BoardEventSystem& boardEventSys, GUIEventSystem& guiEventSys,
    IncommingNetworkEventSystem& incNetworkEventSys) : mBoardEventPublisher {boardEventSys}
{
    subToEvents(guiEventSys, incNetworkEventSys);

    loadFENIntoBoard(startingPosFEN);
    
    //update the pieces internal legal moves
    updateLegalMoves();
}

//Helper function to reduce constructor size.
void Board::subToEvents(GUIEventSystem& guiEventSys, IncommingNetworkEventSystem& incNetworkEventSys)
{
    guiEventSys.sub<GUIEvents::ResetBoard>([this](Event& e)
    {
        resetBoard();
    });

    guiEventSys.sub<GUIEvents::PromotionEnd>([this](Event& e)
    {
        auto const& evnt {static_cast<GUIEvents::PromotionEnd&>(e)};
        mLastMoveMade.promoType = evnt.promoType;
        postMoveUpdate();
    });

    incNetworkEventSys.sub<IncommingNetworkEvents::PairingComplete>([this](Event& e)
    {
        auto const& evnt {static_cast<IncommingNetworkEvents::PairingComplete&>(e)};
        setSideUserIsPlayingAs(evnt.side);
    });

    incNetworkEventSys.sub<IncommingNetworkEvents::OpponentMadeMove>([this](Event& e)
    {
        auto const& evnt {static_cast<IncommingNetworkEvents::OpponentMadeMove&>(e)};
        movePiece(evnt.move);
        postMoveUpdate();
    });
}

void Board::resetBoard()
{
    for(int i = 0; i < 64; ++i) 
        capturePiece(index2ChessPos(i));

    loadFENIntoBoard(startingPosFEN);
    setLastCapturedPiece(nullptr);
    updateLegalMoves();
    mLastMoveMade = ChessMove{};
}

//factory method for placing a piece at the specified location on the board
template<typename ConcreteTy>
void Board::makeNewPieceAt(Vec2i const& pos, Side const side)
{
    //if there is a piece at pos then capture it first
    if(getPieceAt(pos))
        capturePiece(pos);

    try
    {
        m_pieces[chessPos2Index(pos)] = std::make_shared<ConcreteTy>(side, pos);
    }
    catch(std::bad_alloc const& ba)
    {
        (void)ba;//silences c4101 (unused local variable) (cant use [[maybe unused]] here...)
        std::string outOfMemMsg{"problem allocating memory for a "};
        outOfMemMsg.append(typeid(ConcreteTy).name());
        FileErrorLogger::get().log(outOfMemMsg);
    }
}

//Loads up a FEN string into the board. 
//Makes a few assumptions that the given string is a valid FEN string.
//In the future I will probably make a seperate class for loading the
//different portions of a fen string; in order to break up this method which is a bit lengthy.
void Board::loadFENIntoBoard(std::string_view fenString)
{
    //Start at the top left (from white's perspective (a8)).
    int file{0}, rank{7};

    auto it { fenString.cbegin() };

    int numWhiteKings{0}, numBlackKings{0};

    //this loop handles the first field (positional field) in the FEN string
    for(; it != fenString.cend(); ++it)
    {
        if(*it == ' ')//if the next field was reached
        {
            ++it;
            break;
        }
        if(*it == '/')
        {
            --rank;
            file = 0;
        }
        else if(isdigit(*it))
        {
            //ascii digit - 48 maps the char to the cooresponding decimal value
            file += *it - 48;
        }
        else//If the char pointed to by "it" is an upper or lower case letter.
        {
            Side side = isupper(*it) ? Side::WHITE : Side::BLACK;

            switch(tolower(*it))
            {
            case 'p': makeNewPieceAt<Pawn>  ({file, rank}, side); break;
            case 'n': makeNewPieceAt<Knight>({file, rank}, side); break;
            case 'r': makeNewPieceAt<Rook>  ({file, rank}, side); break;
            case 'b': makeNewPieceAt<Bishop>({file, rank}, side); break;
            case 'q': makeNewPieceAt<Queen> ({file, rank}, side); break;
            case 'k':
            {
                makeNewPieceAt<King>({file, rank}, side);
                if(side == Side::WHITE) ++numWhiteKings;
                else ++numBlackKings;
            }
            }
            ++file;
        }
    }

    if(numBlackKings != 1)
        FileErrorLogger::get().log("Error loading the FEN string (incorrect num of black kings)");

    if(numWhiteKings != 1)
        FileErrorLogger::get().log("Error loading the FEN string (incorrect num of white kings)");

    //if the FEN string only had a positional field
    if(it == fenString.cend())
        return;

    //Handle who's turn it is to move.
    //If c is 'w' then it's whites turn otherwise it's blacks turn.
    mWhiteOrBlacksTurn = (*it == 'w') ? Side::WHITE : Side::BLACK;

    //If the turn field wasn't the last field, then jump over the space after it.
    if(++it != fenString.cend())
        ++it;
    else return;

    //handle castling rights
    const Vec2i a1{0, 0}, a8{0, 7}, h1{7, 0}, h8{7, 7};
    for(; it != fenString.cend(); ++it)
    {
        if(*it == ' ')
            break;

        using enum CastleRights;
        switch(*it)
        {
        case 'K'://if the FEN string has white king side castling encoded into it
        {   
            auto const unmovedRook = std::dynamic_pointer_cast<Rook>(getPieceAt(h1));
            assert(unmovedRook);//if white can castle short there should be a rook here
            unmovedRook->setKingOrQueenSide(Rook::KingOrQueenSide::KING_SIDE);
            unmovedRook->setIfRookHasMoved(false);
            m_castlingRights |= WSHORT; 
            break;
        }
        case 'Q':
        {
            auto const unmovedRook = std::dynamic_pointer_cast<Rook>(getPieceAt(a1));
            assert(unmovedRook);//if white can castle long there should be a rook here
            unmovedRook->setKingOrQueenSide(Rook::KingOrQueenSide::QUEEN_SIDE);
            unmovedRook->setIfRookHasMoved(false);
            m_castlingRights |= WLONG;
            break;
        }
        case 'k':
        {   
            auto const unmovedRook = std::dynamic_pointer_cast<Rook>(getPieceAt(h8));
            assert(unmovedRook);//if black can castle short there should be a rook here
            unmovedRook->setKingOrQueenSide(Rook::KingOrQueenSide::KING_SIDE);
            unmovedRook->setIfRookHasMoved(false);
            m_castlingRights |= BSHORT;
            break;
        }
        case 'q':
        {   
            auto const unmovedRook = std::dynamic_pointer_cast<Rook>(getPieceAt(a8));
            assert(unmovedRook);//if black can castle long there should be a rook here
            unmovedRook->setKingOrQueenSide(Rook::KingOrQueenSide::QUEEN_SIDE);
            unmovedRook->setIfRookHasMoved(false);
            m_castlingRights |= BLONG;
        }
        }
    }

    //if the castle rights field wasnt the last field then jump over the space after it
    if(it != fenString.cend())
        ++it;
    else return;

    //handle possible en passant target
    if(*it != '-')
    {
        char c = tolower(*it);
        int file = c - 'a';//map lowcase ascii to 0-26
        ++it;
        int rank = *it - 48;
        --rank;//rank and file start from zero so decrement is needed here
        mEnPassantLocation = {file, rank};
    }

    //if the FEN string doesnt have the last two fields
    if(it == fenString.cend())
        return;
}

void Board::removeCastlingRights(CastleRights const cr)
{
    m_castlingRights &= ~cr;
}

bool Board::hasCastleRights(CastleRights const cr) const
{
    return static_cast<bool>(m_castlingRights & cr);
}

void Board::pickUpPiece(Vec2i const chessPos) const
{
    if(Piece::getPieceOnMouse())
        return;

    auto const p = getPieceAt(chessPos);
    if(p && p->getSide() == getWhosTurnItIs())
        Piece::setPieceOnMouse(p);
}

//called from piecePutDownRoutine() to see if the move being requested
//is in the vector of fully legal moves for the piece on the mouse
auto Board::requestMove(Vec2i const& destinationSquare)
{
    auto pom = Piece::getPieceOnMouse();

    auto end = pom->getLegalMoves().end();
    for(auto it = pom->getLegalMoves().begin(); it != end; ++it)
    {
        if(it->dest == destinationSquare)
            return it;
    }

    return end;
}

void Board::putPieceDown(Vec2i const chessPos)
{
    auto const pom {Piece::getPieceOnMouse()};
    if( ! pom ) { return; }
    
    if( ! isValidChessPosition(chessPos) )
    {
        //Put the piece being held by the mouse back down from where it was picked up.
        Piece::resetPieceOnMouse();
        return;
    }

    if(auto it {requestMove(chessPos)}; it != pom->getLegalMoves().end())
    {
        movePiece(*it);

        if(it->moveType == ChessMove::MoveTypes::PROMOTION || 
            it->moveType == ChessMove::MoveTypes::ROOK_CAPTURE_AND_PROMOTION)
        {
            BoardEvents::PromotionBegin evnt{.promotingSide = getWhosTurnItIs(), .promotingSquare = it->dest};
            mBoardEventPublisher.pub(evnt);

            Piece::resetPieceOnMouse();

            //Return and do the post move update yet. Wait for the user to select a peice from the 
            //promotion popup first. Then there will be enough information to call postMoveUpdate().
            return;
        }

        postMoveUpdate();
    }

    Piece::resetPieceOnMouse();
}

std::shared_ptr<Piece> Board::getPieceAt(Vec2i const& chessPos) const&
{
    return m_pieces[chessPos2Index(chessPos)];
}

void Board::handleKingMove()
{
    auto const king = getPieceAt(mLastMoveMade.dest);
    bool const wasKingWhite = king->getSide() == Side::WHITE;
    using enum CastleRights;
    auto const bitMask = wasKingWhite ? (WLONG | WSHORT) : (BLONG | BSHORT); 
    removeCastlingRights(static_cast<CastleRights>(bitMask));
}

//handle the most recent capture of a rook
void Board::handleRookCapture()
{
    assert(m_lastCapturedPiece);
    auto const rook = std::dynamic_pointer_cast<Rook>(m_lastCapturedPiece);
    assert(rook);

    bool const wasRookWhite = rook->getSide() == Side::WHITE;
    auto const koqs = rook->getKingOrQueenSide();
    using enum Rook::KingOrQueenSide;

    if(koqs == NEITHER) return;

    if(koqs == KING_SIDE)
    {
        removeCastlingRights(wasRookWhite ?
            CastleRights::WSHORT : CastleRights::BSHORT);
    }
    else//if queen side rook
    {
        removeCastlingRights(wasRookWhite ?
            CastleRights::WLONG : CastleRights::BLONG);
    }
}

void Board::handleRookMove()
{
    auto const rook = std::dynamic_pointer_cast<Rook>(getPieceAt(mLastMoveMade.dest));
    assert(rook);

    using enum Rook::KingOrQueenSide;
    auto const koqs = rook->getKingOrQueenSide();
    bool const isRookWhite = rook->getSide() == Side::WHITE;

    //if the rook moved already then we can just leave
    if(rook->getHasMoved() || koqs == NEITHER) return;

    if(koqs == KING_SIDE)
    {
        removeCastlingRights(isRookWhite ? 
            CastleRights::WSHORT : CastleRights::BSHORT);
    }
    else//if queen side rook
    {
        removeCastlingRights(isRookWhite ? 
            CastleRights::WLONG : CastleRights::BLONG);
    }

    rook->setIfRookHasMoved(true);
    rook->setKingOrQueenSide(NEITHER);
}

void Board::handleDoublePushMove()
{
    Vec2i const newEnPassantTarget
    {
        .x = mLastMoveMade.dest.x, 
        .y = mLastMoveMade.dest.y == 3 ? 2 : 5
    };

    updateEnPassant(newEnPassantTarget);
}

void Board::handleCastleMove()
{   
    //the king is on the castlesquare but the rook has yet to be moved

    bool const wasLongCastle = mLastMoveMade.dest.x == 2;

    //where is the rook (before we put it on the other side of the king)
    Vec2i const preCastleRookPos
    {   
        .x = wasLongCastle ? (mLastMoveMade.dest.x - 2) : (mLastMoveMade.dest.x + 1),
        .y = mLastMoveMade.dest.y
    };

    auto rookToBeMoved = getPieceAt(preCastleRookPos);
    bool wasRookWhite = rookToBeMoved->getSide() == Side::WHITE;

    Vec2i const postCastleRookPos
    {
        .x = mLastMoveMade.dest.x + (wasLongCastle ? 1 : -1),
        .y = mLastMoveMade.dest.y
    };

    //Move the rook to the other side of the king.
    movePiece(ChessMove{preCastleRookPos, postCastleRookPos, ChessMove::MoveTypes::CASTLE, 
        false, ChessMove::PromoTypes::INVALID});

    using enum CastleRights;     
    auto const bitMask = wasRookWhite ? (WLONG | WSHORT) : (BLONG | BSHORT);
    removeCastlingRights(static_cast<CastleRights>(bitMask));
}

void Board::handleEnPassantMove()
{
    Vec2i const enPassantSquare = getEnPassantLocation();
    Vec2i const doublePushedPawnPosition
    {
        enPassantSquare.x, 
        enPassantSquare.y == 2 ? 3 : 4
    };

    capturePiece(doublePushedPawnPosition);
}

void Board::postMoveUpdate()
{
    switch(mLastMoveMade.moveType)
    {
    case ChessMove::MoveTypes::DOUBLE_PUSH:   { handleDoublePushMove(); break; }
    case ChessMove::MoveTypes::ENPASSANT:     { handleEnPassantMove();  break; }
    case ChessMove::MoveTypes::CASTLE:        { handleCastleMove();     break; }
    case ChessMove::MoveTypes::ROOK_MOVE:     { handleRookMove();       break; }
    case ChessMove::MoveTypes::ROOK_CAPTURE:  { handleRookCapture();    break; }
    case ChessMove::MoveTypes::KING_MOVE:     { handleKingMove();       break; }
    case ChessMove::MoveTypes::ROOK_CAPTURE_AND_PROMOTION: { handleRookCapture(); [[fallthrough]]; } 
    case ChessMove::MoveTypes::PROMOTION:
    {
        capturePiece(mLastMoveMade.dest);//capture the pawn before we make the new piece

        auto const& pType {mLastMoveMade.promoType};//shorter name

        assert(pType != ChessMove::PromoTypes::INVALID);

        auto const whosTurn = getWhosTurnItIs();

        if(pType == ChessMove::PromoTypes::QUEEN) makeNewPieceAt<Queen>(mLastMoveMade.dest, whosTurn);
        else if(pType == ChessMove::PromoTypes::ROOK) makeNewPieceAt<Rook>(mLastMoveMade.dest, whosTurn);
        else if(pType == ChessMove::PromoTypes::KNIGHT) makeNewPieceAt<Knight>(mLastMoveMade.dest, whosTurn);
        else /*if pType is a bishop promotion*/ makeNewPieceAt<Bishop>(mLastMoveMade.dest, whosTurn);

        Piece::resetPieceOnMouse();
        break;
    }
    }

    if(mLastMoveMade.moveType != ChessMove::MoveTypes::DOUBLE_PUSH)
        resetEnPassant();

    BoardEvents::MoveCompleted evnt{.move = mLastMoveMade};
    mBoardEventPublisher.pub(evnt);

    toggleTurn();
    setLastCapturedPiece(nullptr);
    updateLegalMoves();

    //Determine if the last move caused a checkmate/stalemate.
    //std::nullopt means no check/stalemate has occurred.
    if(auto maybeCheckOrStaleMate{hasCheckOrStalemateOccurred()})
    {
        std::string gameOverReason {*maybeCheckOrStaleMate == MateTypes::CHECKMATE ? 
            "You lost by checkmate" : "Draw by stalemate"};
        
        BoardEvents::GameOver evnt {.reason = std::move(gameOverReason)};//careful! gameOverReason has been moved from
        mBoardEventPublisher.pub(evnt);
    }
}

//std::nullopt means no check/stalemate has occurred.
std::optional<Board::MateTypes> Board::hasCheckOrStalemateOccurred()
{
    //if there are any legal moves for getWhosTurnItIs() side then there has not been a check/slate mate yet
    for(auto const& p : m_pieces)
    {
        if(p && p->getSide() == getWhosTurnItIs())
        {
            if(p->getLegalMoves().size() > 0)
                return std::nullopt;
        }
    }

    //else if there are no legal moves for the other side...
    
    auto const cs = getCheckState();
    assert(cs != CheckType::INVALID);

    MateTypes ret {MateTypes::INVALID};
    
    if(cs == CheckType::SINGLE_CHECK || cs == CheckType::DOUBLE_CHECK)
        ret = MateTypes::CHECKMATE;
    else //if white/black is not in check and they have no legal moves then a stalemate has occurred.
        ret = MateTypes::STALEMATE;

    return ret;
}

void Board::updateEnPassant(Vec2i const newLocation)
{
    mEnPassantLocation = isValidChessPosition(newLocation) ? newLocation : INVALID_VEC2I;
}

void Board::toggleTurn()
{
    mWhiteOrBlacksTurn = mWhiteOrBlacksTurn == Side::WHITE ? Side::BLACK : Side::WHITE;
}

//update the pieces m_pseudoLegals and m_attackedSquares
void Board::updatePseudoLegalsAndAttackedSquares()
{
    for(auto const& p : m_pieces)
    {   
        if(p)
        {
            //since the below method is two methods in one for DRY reasons,
            //its easier to just update both sides every move. This is basically
            //the same amount of work as updating the side that just moved attacked squares,
            //then the side that is about to move psuedo legal moves since it has to do the same
            //work for both of those at once.
            p->updatePseudoLegalAndAttacked(*this);
        }
    }
}

std::vector<Vec2i> Board::getAttackedSquares(Side side) const
{
    std::vector<Vec2i> ret{};
    for(auto const& p : m_pieces)
    {
        if(p && side == p->getSide())
        {
            auto cbegin = p->getAttackedSquares().cbegin();
            auto cend = p->getAttackedSquares().cend();
            ret.insert(ret.cend(), cbegin, cend);
        }
    }
    return ret;
}

//calculate all the pinned pieces for the side that is about to move (toggleTurn() should have already been called)
//returns a vector or pairs where the first piece* is the pinned piece and the other is the pinning piece
void Board::updatePinnedPieces()
{
    for(auto const& p : m_pieces)
    {
        if(!p) continue;

        //this function only needs update the pieces for the side that is about to move
        if(p->getSide() != mWhiteOrBlacksTurn) continue;
        
        p->updatePinnedInfo(*this);
    }
}

void Board::updateCheckState()
{
    mCurrentCheckType = CheckType::NO_CHECK;
    mCheckingPieceLocation = INVALID_VEC2I;
    auto const kingPos
    {
        mWhiteOrBlacksTurn == Side::WHITE ?
        King::getWhiteKingPos() :
        King::getBlackKingPos()
    };
    
    for(auto const& p : m_pieces)
    {
        if(p && p->getSide() != mWhiteOrBlacksTurn)
        {
            auto const cbegin { p->getAttackedSquares().cbegin() };
            auto const cend   { p->getAttackedSquares().cend() };
            if(std::find(cbegin, cend, kingPos) != cend)
            {
                if(mCurrentCheckType == CheckType::SINGLE_CHECK)
                {
                    mCurrentCheckType = CheckType::DOUBLE_CHECK;
                    return;
                }
                
                mCurrentCheckType = CheckType::SINGLE_CHECK;
                mCheckingPieceLocation = p->getChessPosition();
            }
        }
    }
}

void Board::updateLegalMoves()
{
    //1) update all of the piece's psuedo legal moves and attacked squares
    updatePseudoLegalsAndAttackedSquares();

    //2) after we know which squares are being attacked
    //   and the updated pseudo legal moves. We can determine
    //   if the king is in check.
    updateCheckState();

    //3) now update all of the pieces Piece::m_locationOfPiecePinningThis
    updatePinnedPieces();

    //4) now that all of the pieces pseudo legal moves, attacked 
    //   squares, pinned pieces, and m_checkState are up to 
    //   date we can calculate the fully legal moves
    for(auto const& p : m_pieces)
    {
        if(p && p->getSide() == mWhiteOrBlacksTurn)
            p->updateLegalMoves(*this);
    }
}

void Board::capturePiece(Vec2i location)
{  
    assert(isValidChessPosition(location));
    m_lastCapturedPiece = getPieceAt(location);
    m_pieces[chessPos2Index(location)].reset();
}

//all piece moves should go through this method
void Board::movePiece(ChessMove const& move)
{   
    //if there is a piece at the destination move it to the array of captured pieces
    if(getPieceAt(move.dest))
        capturePiece(move.dest);

    auto const destIdx   {chessPos2Index(move.dest)};
    auto const sourceIdx {chessPos2Index(move.src)};

    m_pieces[destIdx] = m_pieces[sourceIdx];
    m_pieces[sourceIdx].reset();

    m_pieces[destIdx]->setChessPosition(move.dest);

    mLastMoveMade = move;
}

//tells if a chess position is on the board or not
bool Board::isValidChessPosition(Vec2i chessPos)
{
    return chessPos.x <= 7 && chessPos.x >= 0 && chessPos.y <= 7 && chessPos.y >= 0;
}
