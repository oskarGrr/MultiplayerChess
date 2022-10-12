#include "Board.h"
#include "SDL.h"
#include "PieceTypes.h"
#include "ChessApplication.h"
#include <string>
#include <exception>
#include <fstream>
#include <cassert>

Board::Board()
    : m_livePieces{}, m_capturedPieces{}, m_checkState(CheckState::INVALID),
      m_locationOfCheckingPiece{-1,-1},   m_enPassantPosition{-1,-1},
      m_sideOfWhosTurnItIs(Side::WHITE),  m_castlingRights{CastleRights::NONE}, 
      m_viewingPerspective(Side::WHITE)
{
    //load the board with the fen string
    std::string const startingFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); 
    loadFENIntoBoard(startingFEN);

    //update the pieces internal legal moves
    updateLegalMoves();
}

//give the pieces back :(
Board::~Board()
{
    for(auto piece : m_livePieces) delete piece;    
    for(auto piece : m_capturedPieces) delete piece;    
}

//factory method for placing a piece at the specified location on the board
//these should only be called in the boards constructor ideally to follow RAII.
//except for when there is a call to either ConreteTy = Queen, Rook, Knight, or Bishop upon promotion.
//all of the memory will be freed in the boards destructor
template<typename ConcreteTy>
void Board::makeNewPieceAt(Vec2i const& pos, Side const side)
{
    //this shouldnt happen but if it does just move the piece to the captured pool
    if(getPieceAt(pos))
        capturePiece(pos);

    try
    {
        m_livePieces[ChessApp::chessPos2Index(pos)] = new ConcreteTy(side, pos);
    }
    catch(std::bad_alloc& ba)
    {
        std::ofstream ofs("log.txt");
        ofs << "problem allocating memory for a " << typeid(ConcreteTy).name();
        ofs.close();
    }
}

//this function can be used to load a FEN with less than 6 fields
//(like just the pysical positions of the pieces). but if it detects
//more than 6 fields in the FEN string it will throw and exception
void Board::loadFENIntoBoard(std::string const& FEN)
{   
    int file = 0, rank = 7;
    auto it = FEN.cbegin();

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
            file=0;
        }
        else if(isdigit(*it))
        {
            //ascii digit - 48 maps the char to the cooresponding decimal value
            file += *it - 48;
        }
        else//if c is an upper or lowercase letter
        {
            Side side = isupper(*it) ? Side::WHITE : Side::BLACK;
            char c = tolower(*it);

            switch(c)
            {
            case 'p': makeNewPieceAt<Pawn>  ({file, rank}, side); break;
            case 'n': makeNewPieceAt<Knight>({file, rank}, side); break;
            case 'r': makeNewPieceAt<Rook>  ({file, rank}, side); break;
            case 'b': makeNewPieceAt<Bishop>({file, rank}, side); break;
            case 'q': makeNewPieceAt<Queen> ({file, rank}, side); break;
            case 'k': makeNewPieceAt<King>  ({file, rank}, side);
            }

            ++file;
        }
    }

    //if the FEN string only had a positional field
    if(it == FEN.cend())
        return;

    //handle whos turn it is to move
    //if c is 'w' then its whites turn otherwise its blacks
    m_sideOfWhosTurnItIs = (*it == 'w') ? Side::WHITE : Side::BLACK;

    //if the turn field wasnt the last field then jump over the space after it
    if(++it != FEN.cend())
        ++it;
    else return;

    //handle castling rights
    constexpr Vec2i a1{0, 0}, a8{0, 7}, h1{7, 0}, h8{7, 7};
    for(; it != FEN.cend(); ++it)
    {
        if(*it == ' ')
            break;

        using enum CastleRights;
        switch(*it)
        {
        case 'K'://if the FEN string has white king side castling encoded into it
        {   
            Rook *const unmovedRook = dynamic_cast<Rook*>(getPieceAt(h1));               
            assert(unmovedRook);
            unmovedRook->setKingOrQueenSide(Rook::KingOrQueenSide::KING_SIDE);
            unmovedRook->setIfRookHasMoved(false);
            m_castlingRights |= WSHORT; 
            break;
        }
        case 'Q':
        {
            Rook *const unmovedRook = dynamic_cast<Rook*>(getPieceAt(a1));
            assert(unmovedRook);   
            unmovedRook->setKingOrQueenSide(Rook::KingOrQueenSide::QUEEN_SIDE);
            unmovedRook->setIfRookHasMoved(false);
            m_castlingRights |= WLONG;
            break;
        }
        case 'k':
        {   
            Rook *const unmovedRook = dynamic_cast<Rook*>(getPieceAt(h8));
            assert(unmovedRook);
            unmovedRook->setKingOrQueenSide(Rook::KingOrQueenSide::KING_SIDE);
            unmovedRook->setIfRookHasMoved(false);
            m_castlingRights |= BSHORT;
            break;
        }
        case 'q':
        {   
            Rook *const unmovedRook = dynamic_cast<Rook*>(getPieceAt(a8));
            assert(unmovedRook);
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
    m_castlingRights &= static_cast<CastleRights>(~static_cast<Uint32>(cr));
}

bool Board::hasCastleRights(CastleRights const cr) const
{
    return static_cast<bool>(m_castlingRights & cr);
}

void Board::piecePickUpRoutine(SDL_Event const& mouseEvent) const
{
    if(mouseEvent.button.button != SDL_BUTTON_LEFT || Piece::getPieceOnMouse())
        return;

    Vec2i screenPos = {mouseEvent.button.x, mouseEvent.button.y};

    if(!ChessApp::isPositionOnBoard(screenPos))
        return;

    Vec2i chessPos = ChessApp::screen2ChessPos(screenPos);

    Piece *const p = getPieceAt(chessPos);

    if(p && p->getSide() == m_sideOfWhosTurnItIs)
        Piece::setPieceOnMouse(p);
}

auto Board::requestMove(Vec2i requestedPosition)
{
    Piece const *const pom = Piece::getPieceOnMouse();
    auto const beg = pom->getLegalMoves().cbegin();
    auto const end = pom->getLegalMoves().cend();
    
    if(!pom) return end;

    for(auto it = beg; it != end; ++it)
    {
        if(it->first == requestedPosition)
            return it;
    }

    return end;
}

void Board::piecePutDownRoutine(SDL_Event const& mouseEvent)
{  
    const Piece* const pom = Piece::getPieceOnMouse();
    if(mouseEvent.button.button != SDL_BUTTON_LEFT || !pom)
        return;

    Vec2i screenPos = {mouseEvent.button.x, mouseEvent.button.y};

    if(!ChessApp::isPositionOnBoard(screenPos))
    { 
        Piece::setPieceOnMouse(nullptr);//put the piece down
        return;
    }

    Vec2i chessPos{ChessApp::screen2ChessPos(screenPos)};

    const auto it = requestMove(chessPos);
    if(it != pom->getLegalMoves().end())
        commitMove(*it);
    
    Piece::setPieceOnMouse(nullptr);//put the piece down
}

void Board::handlePromotionMove(Vec2i const promotionSquare)
{
    Piece const *const pawnToPromote = getPieceAt(promotionSquare);

    //stop here and render popup window with options for which piece to promote to
    ChessApp::promotionRoutine(promotionSquare, pawnToPromote->getSide());
}

void Board::handleKingMove(Vec2i const newKingPos)
{
    Piece const *const king = getPieceAt(newKingPos);
    bool const wasKingWhite = king->getSide() == Side::WHITE;
    using enum CastleRights;
    auto const bitMask = wasKingWhite ? (WLONG | WSHORT) : (BLONG | BSHORT); 
    removeCastlingRights(static_cast<CastleRights>(bitMask));
}

//handle the most recent capture of a rook
void Board::handleRookCapture()
{
    assert(!m_capturedPieces.empty());
    Rook const *const rook = dynamic_cast<Rook*>(m_capturedPieces.back());
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

void Board::handleRookMove(Vec2i const newRookPos)
{
    Rook *const rook = dynamic_cast<Rook*>(getPieceAt(newRookPos));
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

void Board::handleDoublePushMove(Vec2i const doublePushPosition)
{
    Vec2i const newEnPassantTarget
    {
        .x = doublePushPosition.x, 
        .y = doublePushPosition.y == 3 ? 2 : 5
    };

    updateEnPassant(newEnPassantTarget);
}

//castle square is the location of where the king lands after a castle.
void Board::handleCastleMove(Vec2i const castleSquare)
{   
    //the king is on the castlesquare but the rook has yet to be updated

    bool const wasLongCastle = castleSquare.x == 2;

    //where is the rook (before we put it on the other side of the king)
    Vec2i const oldRookPosition
    {   
        .x = wasLongCastle ? (castleSquare.x - 2) : (castleSquare.x + 1),
        .y = castleSquare.y
    };

    Piece* rookToBeMoved = getPieceAt(oldRookPosition);
    bool wasRookWhite = rookToBeMoved->getSide() == Side::WHITE;

    Vec2i const newRookPosition
    {
        .x = castleSquare.x + (wasLongCastle ? 1 : -1),
        .y = castleSquare.y
    };

    movePiece(oldRookPosition, newRookPosition);

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

void Board::commitMove(Move const& newMove)
{
    movePiece(Piece::getPieceOnMouse()->getChessPosition(), newMove.first);
    postMoveUpdate(newMove);
}

void Board::postMoveUpdate(Move const& newMove)
{
    const auto& [move, moveType] = newMove;
    using enum MoveInfo;

    switch(moveType)
    {
    case PROMOTION:     handlePromotionMove(move);  break;
    case DOUBLE_PUSH:   handleDoublePushMove(move); break;
    case ENPASSANT:     handleEnPassantMove();      break;
    case CASTLE:        handleCastleMove(move);     break;
    case ROOK_MOVE:     handleRookMove(move);       break;
    case ROOK_CAPTURE:  handleRookCapture();        break;
    case KING_MOVE:     handleKingMove(move);       break;
    case ROOK_CAPTURE_AND_PROMOTION:
    {
        handleRookCapture();
        handlePromotionMove(move);
    }
    }                   

    if(moveType != DOUBLE_PUSH)
        resetEnPassant();

    toggleTurn();
    updateLegalMoves();
}

void Board::updateEnPassant(Vec2i const newLocation)
{
    m_enPassantPosition = ChessApp::inRange(newLocation) ? newLocation : Vec2i{-1, -1};
}

void Board::toggleTurn()
{
    m_sideOfWhosTurnItIs = m_sideOfWhosTurnItIs == Side::WHITE ? Side::BLACK : Side::WHITE;
}

//update the pieces m_pseudoLegals and m_attackedSquares
void Board::updatePseudoLegalsAndAttackedSquares()
{
    for(Piece *const p : m_livePieces)
    {   
        if(p)
        {
            //theoretically we should only need to get the pseudo legal moves for
            //the side that is about to move and the attacked squares for the side that just moved.
            //but since they are combined into one function for DRY reasons,
            //It would be much easier to just update both sides every move.
            p->updatePseudoLegalAndAttacked();
        }
    }
}

std::vector<Vec2i> Board::getAttackedSquares(Side side) const
{
    std::vector<Vec2i> ret{};
    for(Piece const *const p : m_livePieces)
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
    for(Piece *const p : m_livePieces)
    {
        if(!p) continue;

        //this function only needs update the pieces for the side that is about to move
        if(p->getSide() != m_sideOfWhosTurnItIs) continue;
        
        p->updatePinnedInfo();
    }
}

void Board::updateCheckState()
{
    using enum CheckState;
    Vec2i const kingPos = m_sideOfWhosTurnItIs == Side::WHITE ?
        King::getWhiteKingPos() : King::getBlackKingPos();

    m_checkState = NO_CHECK;
    m_locationOfCheckingPiece = {-1, -1};
    
    for(Piece const *const p : m_livePieces)
    {
        if(p && p->getSide() != m_sideOfWhosTurnItIs)
        {
            auto const cbegin = p->getAttackedSquares().cbegin(), cend = p->getAttackedSquares().cend();
            auto possibleSingleCheck = std::find(cbegin, cend, kingPos);

            if(possibleSingleCheck != cend)
            {
                m_checkState = SINGLE_CHECK;
                m_locationOfCheckingPiece = p->getChessPosition();
                //the king is in at least CheckState::SINGLE_CHECK
                //now check for CheckState::DOUBLE_CHECK

                //if there is a possiblity for there to be a double check
                if(++possibleSingleCheck != cend)
                {
                    //if we are in double check
                    if(std::find(possibleSingleCheck, cend, kingPos) != cend)
                        m_checkState = DOUBLE_CHECK;
                }
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

    //updates all of the pieces Piece::m_locationOfPiecePinningThis
    updatePinnedPieces();

    //now that all of the pieces pseudo legal moves, attacked 
    //squares, pinned pieces, and m_checkState are up to 
    //date we can calculate the fully legal moves
    for(Piece *const p : m_livePieces)
    {
        if(p && p->getSide() == m_sideOfWhosTurnItIs)
            p->updateLegalMoves();
    }
}

void Board::capturePiece(Vec2i const location)
{  
    Piece *const piece = getPieceAt(location);

    if(!piece) return;//if there isnt a piece here just leave
    
    m_capturedPieces.push_back(piece);
    m_livePieces[ChessApp::chessPos2Index(location)] = nullptr;
}

//all piece moves should go through this function 
void Board::movePiece(Vec2i const source, Vec2i const destination)
{   
    //if there is a piece at the destination move it to the array of captured pieces
    if(getPieceAt(destination))
        capturePiece(destination);

    int const idest   = ChessApp::chessPos2Index(destination);
    int const isource = ChessApp::chessPos2Index(source);

    m_livePieces[idest]   = m_livePieces[isource];
    m_livePieces[isource] = nullptr;

    m_livePieces[idest]->setChessPosition(destination);
}