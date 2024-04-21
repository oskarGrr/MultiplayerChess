#include "Board.h"
#include "SDL.h"
#include "PieceTypes.h"
#include "ChessApplication.h"
#include "errorLogger.h"
#include <string>
#include <exception>
#include <fstream>
#include <cassert>

//#define STARTING_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define STARTING_FEN "7k/8/8/8/8/8/6q1/K7 w - 0 1"//easy to test stalemate pos

Board::Board()
    : m_pieces{}, m_lastCapturedPiece{}, m_checkState(CheckState::INVALID),
      m_locationOfCheckingPiece{INVALID_VEC2I}, m_locationOfSecondCheckingPiece{INVALID_VEC2I},
      m_enPassantPosition{INVALID_VEC2I}, m_sideOfWhosTurnItIs(Side::WHITE),
      m_castlingRights(CastleRights::NONE), m_viewingPerspective(Side::WHITE),
      m_sideUserIsPlayingAs(Side::INVALID), m_lastMoveMade{}
{
    //load the board with the fen string
    std::string const startingFEN(STARTING_FEN);
    loadFENIntoBoard(startingFEN);
    
    //update the pieces internal legal moves
    updateLegalMoves();
}

Board::~Board()
{
    //the references to the pieces on the heap are managed by shared_ptr's so nothing to do here
}

void Board::flipBoardViewingPerspective()
{
    setBoardViewingPerspective(m_viewingPerspective ==
        Side::WHITE ? Side::BLACK : Side::WHITE);
}

void Board::resetBoard()
{
    for(int i = 0; i < 64; ++i) 
        capturePiece(ChessApp::index2ChessPos(i));

    loadFENIntoBoard(STARTING_FEN);
    setLastCapturedPiece(nullptr);
    updateLegalMoves();
    m_lastMoveMade = Move();
    auto& app = ChessApp::getApp();
    app.updateGameState(GameState::GAME_IN_PROGRESS);
    app.closeWinLossDrawWindow();
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
        m_pieces[ChessApp::chessPos2Index(pos)] = std::make_shared<ConcreteTy>(side, pos);
    }
    catch(std::bad_alloc const& ba)
    {
        (void)ba;//silences c4101 (unused local variable) (cant use [[maybe unused]] here...)
        std::string outOfMemMsg{"problem allocating memory for a "};
        outOfMemMsg.append(typeid(ConcreteTy).name());
        FileErrorLogger::get().logErrors(outOfMemMsg);
    }
}

//Loads up a FEN string into the board. 
//Makes a few assumptions that the given string is a valid FEN string.
//In the future I will probably make a seperate class for loading the
//different portions of a fen string; in order to break up this method which is a bit lengthy.
void Board::loadFENIntoBoard(std::string_view FEN)
{
    //Start at the top left (from white's perspective (a8)).
    int file{0}, rank{7};

    auto it = FEN.cbegin();

    int numWhiteKings{0}, numBlackKings{0};

    //this loop handles the first field (positional field) in the FEN string
    for(; it != FEN.cend(); ++it)
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
        throw FenException("Too many or too few black kings in FEN string");

    if(numWhiteKings != 1)
        throw FenException("Too many or too few white kings in FEN string");

    //if the FEN string only had a positional field
    if(it == FEN.cend())
        return;

    //Handle who's turn it is to move.
    //If c is 'w' then it's whites turn otherwise it's blacks turn.
    m_sideOfWhosTurnItIs = (*it == 'w') ? Side::WHITE : Side::BLACK;

    //If the turn field wasn't the last field, then jump over the space after it.
    if(++it != FEN.cend())
        ++it;
    else return;

    //handle castling rights
    const Vec2i a1{0, 0}, a8{0, 7}, h1{7, 0}, h8{7, 7};
    for(; it != FEN.cend(); ++it)
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
    if(it != FEN.cend())
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
        m_enPassantPosition = {file, rank};
    }

    //if the FEN string doesnt have the last two fields
    if(it == FEN.cend())
        return;
}

void Board::removeCastlingRights(CastleRights const cr)
{
    m_castlingRights &= static_cast<CastleRights>(~static_cast<uint32_t>(cr));
}

bool Board::hasCastleRights(CastleRights const cr) const
{
    return static_cast<bool>(m_castlingRights & cr);
}

void Board::piecePickUpRoutine(SDL_Event const& mouseEvent) const
{
    if(mouseEvent.button.button != SDL_BUTTON_LEFT || Piece::getPieceOnMouse())
        return;

    auto& app = ChessApp::getApp();

    if(app.isUserPaired() && getWhosTurnItIs() != getSideUserIsPlayingAs())
        return;

    Vec2i screenPos{mouseEvent.button.x, mouseEvent.button.y};
    if(!ChessApp::isScreenPositionOnBoard(screenPos))
        return;

    Vec2i chessPos = ChessApp::screen2ChessPos(screenPos);

    auto const p = getPieceAt(chessPos);

    if(p && p->getSide() == m_sideOfWhosTurnItIs)
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
        if(it->m_dest == destinationSquare)
            return it;
    }

    return end;
}

void Board::piecePutDownRoutine(SDL_Event const& mouseEvent)
{  
    auto& app = ChessApp::getApp();
    auto const pom = Piece::getPieceOnMouse();
    if(mouseEvent.button.button != SDL_BUTTON_LEFT || !pom)
        return;

    //case where the promotion window is open and the user has yet to choose a piece
    if(app.isPromotionWindowOpen())
        return;
        
    Vec2i screenPos{mouseEvent.button.x, mouseEvent.button.y};

    if(!ChessApp::isScreenPositionOnBoard(screenPos))
    {
        Piece::putPieceOnMouseDown();
        return;
    }

    if(auto it = requestMove(ChessApp::screen2ChessPos(screenPos));
        it != pom->getLegalMoves().end())
    {
        movePiece(*it);
        if(it->m_moveType == MoveInfo::PROMOTION ||
           it->m_moveType == MoveInfo::ROOK_CAPTURE_AND_PROMOTION)
        {
            app.openPromotionWindow();
            return;
        }

        postMoveUpdate(*it);
    }

    Piece::putPieceOnMouseDown();
}

std::shared_ptr<Piece> Board::getPieceAt(Vec2i const& chessPos) const
{
    return m_pieces[ChessApp::chessPos2Index(chessPos)];
}

void Board::handleKingMove(Vec2i const& newKingPos)
{
    auto const king = getPieceAt(newKingPos);
    bool const wasKingWhite = king->getSide() == Side::WHITE;
    using enum CastleRights;
    auto const bitMask = wasKingWhite ? (WLONG | WSHORT) : (BLONG | BSHORT); 
    removeCastlingRights(static_cast<CastleRights>(bitMask));
}

//handle the most recent capture of a rook
void Board::handleRookCapture()
{
    assert(m_lastCapturedPiece);
    auto const rook = std::dynamic_pointer_cast<Rook>(m_lastCapturedPiece);;
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

void Board::handleRookMove(Vec2i const& newRookPos)
{
    auto const rook = std::dynamic_pointer_cast<Rook>(getPieceAt(newRookPos));
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

void Board::handleDoublePushMove(Vec2i const& doublePushPosition)
{
    Vec2i const newEnPassantTarget
    {
        .x = doublePushPosition.x, 
        .y = doublePushPosition.y == 3 ? 2 : 5
    };

    updateEnPassant(newEnPassantTarget);
}

void Board::handleCastleMove(Move const& castleMove)
{   
    //the king is on the castlesquare but the rook has yet to be moved

    bool const wasLongCastle = castleMove.m_dest.x == 2;

    //where is the rook (before we put it on the other side of the king)
    Vec2i const preCastleRookPos
    {   
        .x = wasLongCastle ? (castleMove.m_dest.x - 2) : (castleMove.m_dest.x + 1),
        .y = castleMove.m_dest.y
    };

    auto rookToBeMoved = getPieceAt(preCastleRookPos);
    bool wasRookWhite = rookToBeMoved->getSide() == Side::WHITE;

    Vec2i const postCastleRookPos
    {
        .x = castleMove.m_dest.x + (wasLongCastle ? 1 : -1),
        .y = castleMove.m_dest.y
    };

    movePiece(Move(preCastleRookPos, postCastleRookPos, MoveInfo::CASTLE));

    using enum CastleRights;     
    auto const bitMask = wasRookWhite ? (WLONG | WSHORT) : (BLONG | BSHORT);
    removeCastlingRights(static_cast<CastleRights>(bitMask));
}

void Board::handleEnPassantMove()
{
    Vec2i const enPassantSquare = getEnPassantIndex();
    Vec2i const doublePushedPawnPosition
    {
        enPassantSquare.x, 
        enPassantSquare.y == 2 ? 3 : 4
    };

    capturePiece(doublePushedPawnPosition);
}

//to be called in postMoveUpdate() after the correct above single ha
//method was called and before m_lastCapturedPiece is reset to null
void Board::playCorrectMoveAudio(MoveInfo recentMoveInfo)
{
    auto& app = ChessApp::getApp();
    switch(recentMoveInfo)
    {
    using enum MoveInfo;
    //move sounds
    case DOUBLE_PUSH: [[fallthrough]];
    case ROOK_MOVE:   [[fallthrough]];
    case NORMAL: app.playChessMoveSound(); break;

    //capture sounds
    case ENPASSANT: [[fallthrough]];
    case ROOK_CAPTURE: [[fallthrough]];
    case ROOK_CAPTURE_AND_PROMOTION: [[fallthrough]];
    case NORMAL_CAPTURE: app.playChessCaptureSound(); break;

    //capture or move sound 
    case PROMOTION:   [[fallthrough]];
    case KING_MOVE:
    {
        auto& b = app.getBoard();
        if(b.getLastCapturedPiece())//if the king move captured a piece
            app.playChessCaptureSound();
        else
            app.playChessMoveSound();
        break;
    }
    case CASTLE: app.playChessCastleSound();
    }
}

//whichPromoType is defaulted to PromoType::INVALID to indicate no promotion. otherwise
//whichPromoType should indicate the type of piece being promoted to
void Board::postMoveUpdate(Move const& move, PromoType whichPromoType)
{
    auto const whosTurn = getWhosTurnItIs();

    switch(move.m_moveType)
    {
    case MoveInfo::DOUBLE_PUSH:   handleDoublePushMove(move.m_dest); break;
    case MoveInfo::ENPASSANT:     handleEnPassantMove();             break;
    case MoveInfo::CASTLE:        handleCastleMove(move);            break;
    case MoveInfo::ROOK_MOVE:     handleRookMove(move.m_dest);       break;
    case MoveInfo::ROOK_CAPTURE:  handleRookCapture();               break;
    case MoveInfo::KING_MOVE:     handleKingMove(move.m_dest);       break;
    case MoveInfo::ROOK_CAPTURE_AND_PROMOTION: handleRookCapture();  [[fallthrough]];
    case MoveInfo::PROMOTION:
    {
        capturePiece(move.m_dest);//capture the pawn before we make the new piece
        using enum PromoType;
        assert(whichPromoType != INVALID);
        if(whichPromoType == QUEEN_PROMOTION) makeNewPieceAt<Queen>(move.m_dest, whosTurn);
        else if(whichPromoType == ROOK_PROMOTION) makeNewPieceAt<Rook>(move.m_dest, whosTurn);
        else if(whichPromoType == KNIGHT_PROMOTION) makeNewPieceAt<Knight>(move.m_dest, whosTurn);
        else makeNewPieceAt<Bishop>(move.m_dest, whosTurn);
        Piece::putPieceOnMouseDown();
    }
    }

    if(move.m_moveType != MoveInfo::DOUBLE_PUSH)
        resetEnPassant();

    playCorrectMoveAudio(move.m_moveType);

    auto& app = ChessApp::getApp();
    if(app.isUserPaired() && whosTurn == getSideUserIsPlayingAs())
        app.buildAndSendMoveMsg(move, whichPromoType);

    toggleTurn();
    setLastCapturedPiece(nullptr);//if there was a last captured piece it was consumed already. so set it to null
    updateLegalMoves();
    checkForCheckOrStaleM8(getWhosTurnItIs());
}

//will check for stale or check mate. if either one is true then update the game state to that.
void Board::checkForCheckOrStaleM8(Side const sideToCheck)
{
    for(auto const& p : m_pieces)
    {
        if(p && p->getSide() == sideToCheck)
        {
            if(p->getLegalMoves().size() > 0) return;
        }
    }
    
    auto cs = getCheckState();
    bool isInCheck = cs == CheckState::DOUBLE_CHECK || cs == CheckState::SINGLE_CHECK;
    auto& app = ChessApp::getApp();
    app.updateGameState(isInCheck ? GameState::CHECKMATE : GameState::STALEMATE);
    app.openWinLossDrawWindow();
}

void Board::updateEnPassant(Vec2i const& newLocation)
{
    m_enPassantPosition = ChessApp::inRange(newLocation) ? newLocation : INVALID_VEC2I;
}

void Board::toggleTurn()
{
    m_sideOfWhosTurnItIs = m_sideOfWhosTurnItIs == Side::WHITE ? Side::BLACK : Side::WHITE;
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
            p->updatePseudoLegalAndAttacked();
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
        if(p->getSide() != m_sideOfWhosTurnItIs) continue;
        
        p->updatePinnedInfo();
    }
}

void Board::updateCheckState()
{       
    m_checkState = CheckState::NO_CHECK;//reset m_checkState
    m_locationOfCheckingPiece = INVALID_VEC2I;//reset m_locationOfCheckingPiece
    m_locationOfSecondCheckingPiece = INVALID_VEC2I;
    Vec2i const kingPos = m_sideOfWhosTurnItIs == Side::WHITE ?
        King::getWhiteKingPos() : King::getBlackKingPos();

    for(auto const& p : m_pieces)
    {
        if(p && p->getSide() != m_sideOfWhosTurnItIs)
        {
            auto const cbegin = p->getAttackedSquares().cbegin(), cend = p->getAttackedSquares().cend();
            if(std::find(cbegin, cend, kingPos) != cend)
            {
                if(m_checkState == CheckState::SINGLE_CHECK)
                {
                    m_checkState = CheckState::DOUBLE_CHECK;
                    m_locationOfSecondCheckingPiece = p->getChessPosition();
                    return;
                }
                
                m_checkState = CheckState::SINGLE_CHECK;
                m_locationOfCheckingPiece = p->getChessPosition();
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
        if(p && p->getSide() == m_sideOfWhosTurnItIs)
            p->updateLegalMoves();
    }
}

void Board::capturePiece(Vec2i const& location)
{  
    assert(ChessApp::inRange(location));
    m_lastCapturedPiece = getPieceAt(location);
    m_pieces[ChessApp::chessPos2Index(location)].reset();
}

//all piece moves should go through this method
void Board::movePiece(Move const& move)
{   
    //if there is a piece at the destination move it to the array of captured pieces
    if(getPieceAt(move.m_dest))
        capturePiece(move.m_dest);

    int const idest   = ChessApp::chessPos2Index(move.m_dest);
    int const isource = ChessApp::chessPos2Index(move.m_source);

    m_pieces[idest] = m_pieces[isource];
    m_pieces[isource].reset();

    m_pieces[idest]->setChessPosition(move.m_dest);
    
    m_lastMoveMade = move;
}