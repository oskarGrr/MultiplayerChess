#include "PieceTypes.h"
#include "Board.hpp"
#include "SDL_image.h"
#include "SDL.h"
#include <cassert>
#include <algorithm>//std::foreach

Piece::Piece(Side const side, Vec2i const chessPos)
    : m_pseudoLegals{}, m_legalMoves{}, m_side(side),
      m_chessPos(chessPos), m_attackedSquares{},
      m_whichTexture(TextureManager::WhichTexture::INVALID), m_type(PieceTypes::INVALID),
      m_locationOfPiecePinningThis{INVALID_VEC2I}
{
}

Pawn::Pawn(Side const side, Vec2i const chessPos) : Piece(side, chessPos)
{
    using enum TextureManager::WhichTexture;
    m_type = PieceTypes::PAWN;
    m_whichTexture = side == Side::WHITE ? WHITE_PAWN : BLACK_PAWN;
}

Knight::Knight(Side const side, Vec2i const chessPos) : Piece(side, chessPos)
{
    using enum TextureManager::WhichTexture;
    m_type = PieceTypes::KNIGHT;
    m_whichTexture = side == Side::WHITE ? WHITE_KNIGHT : BLACK_KNIGHT;
}

Rook::Rook(Side const side, Vec2i const chessPos) : Piece(side, chessPos),
    m_hasMoved(true), m_koqs(KingOrQueenSide::NEITHER)
{
    using enum TextureManager::WhichTexture;
    m_type = PieceTypes::ROOK;
    m_whichTexture = side == Side::WHITE ? WHITE_ROOK : BLACK_ROOK;
}

Bishop::Bishop(Side const side, Vec2i const chessPos) : Piece(side, chessPos)
{
    using enum TextureManager::WhichTexture;
    m_type = PieceTypes::BISHOP;
    m_whichTexture = side == Side::WHITE ? WHITE_BISHOP : BLACK_BISHOP;
}

Queen::Queen(Side const side, Vec2i const chessPos) : Piece(side, chessPos)
{
    using enum TextureManager::WhichTexture;
    m_whichTexture = side == Side::WHITE ? WHITE_QUEEN : BLACK_QUEEN;
    m_type = PieceTypes::QUEEN;
}

King::King(Side const side, Vec2i const chessPos) : Piece(side, chessPos)
{
    using enum TextureManager::WhichTexture;
    m_type = PieceTypes::KING;
    m_whichTexture = side == Side::WHITE ? WHITE_KING : BLACK_KING;

    if(side == Side::WHITE) s_wKingPos = m_chessPos;
    else s_bKingPos = m_chessPos;
}

//used by queens & rooks impl of virtual void updatePseudoLegalAndAttacked()=0; 
void Piece::orthogonalSlide(Board const& b)
{
    using enum ChessMove::MoveTypes;

    //slide orthogonally left then right then down then up (from whites perspective)
    for(int i = 0; i < 4; ++i)
    {
        Vec2i offsetPos{m_chessPos};

        if(i < 2)
            offsetPos.x = i == 0 ? m_chessPos.x - 1 : m_chessPos.x + 1;
        else//if i is 2 or 3
            offsetPos.y = i == 2 ? m_chessPos.y - 1 : m_chessPos.y + 1;

        //loops runs as long as the slide stays on the board
        //or the break occures below because the loop ran into a piece
        while(Board::isValidChessPosition(offsetPos))
        {
            auto const piece = b.getPieceAt(offsetPos);

            if( ! piece )//if there isnt a piece add it to the vectors
            {
                m_pseudoLegals.emplace_back(m_chessPos, offsetPos, NORMAL);
                m_attackedSquares.push_back(offsetPos);
            }
            else//if there is a piece here
            {
                //if the piece we ran into is the oposite color then add it to the vector
                if(m_side != piece->getSide())
                {
                    m_pseudoLegals.emplace_back(m_chessPos, offsetPos, getType(*piece) == PieceTypes::ROOK ?
                        ROOK_CAPTURE : NORMAL_CAPTURE);

                    m_attackedSquares.push_back(offsetPos);

                    if(piece->m_type != PieceTypes::KING) 
                        break;//stop sliding.
                }
                else//if the piece is the same color as *slidingPiece
                {
                    m_attackedSquares.push_back(offsetPos);
                    break;//stop sliding
                }
            }

            //slide left or right or down or up depending on i 
            //(0 == left; 1 == right; 2 == down; 3 == up)
            if(i < 2)
                offsetPos.x = i == 0 ? offsetPos.x - 1 : offsetPos.x + 1;
            else//if i is 2 or 3
                offsetPos.y = i == 2 ? offsetPos.y - 1 : offsetPos.y + 1;
        }
    }
}

//used by queens & bishops impl of virtual void updatePseudoLegalAndAttacked()=0; 
void Piece::diagonalSlide(Board const& b)
{
    using enum ChessMove::MoveTypes;

    //slide diagonally bottom left then bottom right 
    //then top right then top left (from whites perspective)
    for(int i = 0; i < 4; ++i)
    {
        Vec2i offsetPos{m_chessPos};

        offsetPos.y = (i < 2) ? offsetPos.y - 1 : offsetPos.y + 1;
        offsetPos.x = (i == 0 || i == 3) ? offsetPos.x - 1 : offsetPos.x + 1;

        //loops runs as long as the diagonal slide stays on the board
        //or the break occures below because the slide ran into a piece
        while(Board::isValidChessPosition(offsetPos))
        {
            auto piece = b.getPieceAt(offsetPos);

            if(!piece)//if there isnt a piece at offsetIndex add it to the vectors
            {
                m_pseudoLegals.emplace_back(m_chessPos, offsetPos, NORMAL);
                m_attackedSquares.push_back(offsetPos);
            }
            else//if there is a piece at offsetIndex
            {
                //if the piece we ran into is the oposite color then add it to the vector
                if(m_side != piece->getSide())
                {
                    m_pseudoLegals.emplace_back(m_chessPos, offsetPos, 
                        getType(*piece) == PieceTypes::ROOK ? ROOK_CAPTURE : NORMAL_CAPTURE);

                    m_attackedSquares.push_back(offsetPos);

                    if(piece->m_type != PieceTypes::KING)
                        break;//stop sliding
                }
                else//if the piece is the same color as *slidingPiece
                {
                    m_attackedSquares.push_back(offsetPos);
                    break;//stop sliding
                }
            }

            if(i < 2)
            {
                --offsetPos.y;
                offsetPos.x = i == 0 ? offsetPos.x - 1 : offsetPos.x + 1;
            }
            else//if i is 2 or 3
            {
                ++offsetPos.y;
                offsetPos.x = i == 2 ? offsetPos.x + 1 : offsetPos.x - 1;
            }
        }
    }
}

//calculate the pseudo legal moves for a pawn and store then in Piece::m_pseudoLegals
//also calculates Piece::m_attackedSquares
void Pawn::updatePseudoLegalAndAttacked(Board const& b)
{
    m_pseudoLegals.clear();
    m_attackedSquares.clear();
    int const yDirection = m_side == Side::WHITE ? 1 : -1;//the direction that the pawn is moving
    Vec2i const oneInFront{m_chessPos.x, m_chessPos.y + yDirection};
    using enum ChessMove::MoveTypes;

    //check the spot right in front of the pawn
    if( ! b.getPieceAt(oneInFront) )
    {
        m_pseudoLegals.emplace_back
        (
            m_chessPos, 
            oneInFront,
            oneInFront.y == 7 || oneInFront.y == 0 ? PROMOTION : NORMAL
        );

        //we know there isnt a piece in front of the pawn
        //lets also check if it hasnt moved yet and if so 
        //then also check if the pawn can perform a double push
        Vec2i const twoInFront{oneInFront.x, oneInFront.y + yDirection};
        if(Board::isValidChessPosition(twoInFront) && !b.getPieceAt(twoInFront))
        {          
            if(m_chessPos.y == 1 && m_side == Side::WHITE)
                m_pseudoLegals.emplace_back(m_chessPos, twoInFront, DOUBLE_PUSH);
            else if(m_chessPos.y == 6 && m_side == Side::BLACK)
                m_pseudoLegals.emplace_back(m_chessPos, twoInFront, DOUBLE_PUSH);
        }
    }

    auto const checkAttackingSquare = [this, &b](Vec2i squareToCheck)
    {
        if( ! Board::isValidChessPosition(squareToCheck) )
            return;

        auto const piece = b.getPieceAt(squareToCheck);
        if(piece)//if there is a piece at squareToCheck
        {
            if(piece->getSide() != m_side)//if piece is of the oposite color to *this
            {
                bool const isPromotion = squareToCheck.y == 7 || squareToCheck.y == 0;
                bool const isRookCapture = getType(*piece) == PieceTypes::ROOK;//did this pawn capture a rook
                if(isPromotion)
                {
                    m_pseudoLegals.emplace_back(m_chessPos, squareToCheck, isRookCapture ? 
                        ROOK_CAPTURE_AND_PROMOTION : PROMOTION);
                }
                else//if squareToCheck is a capture but not a promotion
                {
                    //the only rook capture we care about is one where the rook 
                    //hasnt moved yet. If the capture here is a rook, then the rook has
                    //already moved and so we can just emplace a NORMAL moveInfo enum instead
                    m_pseudoLegals.emplace_back(m_chessPos, squareToCheck, NORMAL_CAPTURE);
                }
            }
        }
        else//if there is not a piece at squareToCheck
        {
            //if the attacking square is the en passant target square
            if(squareToCheck == b.getEnPassantLocation())
                m_pseudoLegals.emplace_back(m_chessPos, squareToCheck, ENPASSANT);
        }

        m_attackedSquares.push_back(squareToCheck);
        
    };

    //next im going to check if the pawn is attacking anything 
    //(see if there is an enemy piece to the top left or the top right of the pawn)
    Vec2i const topLeft{oneInFront.x - 1, oneInFront.y};
    Vec2i const topRight{oneInFront.x + 1, oneInFront.y};
    checkAttackingSquare(topLeft);
    checkAttackingSquare(topRight);
}

//calculate the pseudo legal moves for a knight and store then in Piece::m_pseudoLegals
//also calculates Piece::m_attackedSquares
void Knight::updatePseudoLegalAndAttacked(Board const& b)
{
    m_pseudoLegals.clear();
    m_attackedSquares.clear();
    Vec2i offsetPos{m_chessPos};
    using enum ChessMove::MoveTypes;
    
    auto potentialPushBack = [&b, this](Vec2i offsetPos)
    {
        if( ! Board::isValidChessPosition(offsetPos) )
            return;
        
        auto const piece = b.getPieceAt(offsetPos);
        if( ! piece )
        {
            m_attackedSquares.push_back(offsetPos);
            m_pseudoLegals.emplace_back(m_chessPos, offsetPos, NORMAL);
        }
        else if(m_side != piece->getSide())
        {
            //if there is a piece here and its the oposite color of the knight
            m_attackedSquares.push_back(offsetPos);
            m_pseudoLegals.emplace_back
            (
                m_chessPos,
                offsetPos, 
                getType(*piece) == PieceTypes::ROOK ? ROOK_CAPTURE : NORMAL_CAPTURE
            );
        }
        else//if the piece is the same color as the knight
        {
            m_attackedSquares.push_back(offsetPos);
        }
    };

    //Check all 8 possible chess positions in this order from whites perspective.
    //(X is where the knight is.)
    //                 |_|8|_|7|_|
    //                 |1|_|_|_|6|
    //                 |_|_|X|_|_|
    //                 |2|_|_|_|5|
    //                 |_|3|_|4|_|

    offsetPos.x = m_chessPos.x - 2; offsetPos.y = m_chessPos.y + 1;
    potentialPushBack(offsetPos);//left two up one from knight
    
    offsetPos.y -= 2;
    potentialPushBack(offsetPos);//left two down one from knight
    
    ++offsetPos.x; --offsetPos.y;
    potentialPushBack(offsetPos);//left one and down two from knight

    offsetPos.x += 2;
    potentialPushBack(offsetPos);//right one and down two from kngith

    ++offsetPos.x; ++offsetPos.y;
    potentialPushBack(offsetPos);//right two and down one from knight

    offsetPos.y += 2;
    potentialPushBack(offsetPos);//right two and up one from kngiht

    --offsetPos.x; ++offsetPos.y;
    potentialPushBack(offsetPos);//right one and up two from knight

    offsetPos.x -= 2;
    potentialPushBack(offsetPos);//left one and up two from knight
}

//calculate the pseudo legal moves for a rook and store then in Piece::m_pseudoLegals
//also calculates Piece::m_attackedSquares
void Rook::updatePseudoLegalAndAttacked(Board const& b)
{
    m_pseudoLegals.clear();
    m_attackedSquares.clear();
    orthogonalSlide(b);//slide the piece in all 4 orthogonal directions

    std::for_each(m_pseudoLegals.begin(), m_pseudoLegals.end(),
        [](auto& move)
        {
            if(move.moveType == ChessMove::MoveTypes::NORMAL) 
                move.moveType = ChessMove::MoveTypes::ROOK_MOVE;
        }
    );
}

//calculate the pseudo legal moves for a bishop and store then in Piece::m_pseudoLegals
//also calculates Piece::m_attackedSquares
void Bishop::updatePseudoLegalAndAttacked(Board const& b)
{
    m_pseudoLegals.clear();
    m_attackedSquares.clear();
    diagonalSlide(b);
}

//calculate the pseudo legal moves for a queen and store then in Piece::m_pseudoLegals
//also calculates Piece::m_attackedSquares
void Queen::updatePseudoLegalAndAttacked(Board const& b)
{
    m_pseudoLegals.clear();
    m_attackedSquares.clear();
    diagonalSlide(b);
    orthogonalSlide(b);
}

//calculate the pseudo legal moves for a king and store then in Piece::m_pseudoLegals
//also calculates Piece::m_attackedSquares
void King::updatePseudoLegalAndAttacked(Board const& b)
{
    bool const isWhite = (m_side == Side::WHITE);
    m_attackedSquares.clear();
    m_pseudoLegals.clear();

    //(this is from whites perspective)  
    //check all 8 possible chess positions in this order
    //when the loop gets to position 2 and 7 it will also 
    //check if the king can castle on that side 
    //|3|5|8|                    
    //|2|K|7|                   
    //|1|4|6|                   

    for(int offsetFile = m_chessPos.x - 1; offsetFile < m_chessPos.x + 2; ++offsetFile)
    {
        for(int offsetRank = m_chessPos.y - 1; offsetRank < m_chessPos.y + 2; ++offsetRank)
        {
            using enum ChessMove::MoveTypes;
            Vec2i const offsetPosition{offsetFile, offsetRank};

            if( ! Board::isValidChessPosition(offsetPosition) )
                continue;

            auto const piece = b.getPieceAt(offsetPosition);

            //when the loop gets the the position in between 4 and 5
            //in the diagram above it will be on the same square as the king
            //so continue to the next loop iteration
            if(piece.get() == this)
                continue;                                     

            if( ! piece )//there isnt a piece at the offset position
            {
                m_pseudoLegals.emplace_back(m_chessPos, offsetPosition, KING_MOVE);
                m_attackedSquares.push_back(offsetPosition);

                //check if the location for castling is available
                Vec2i directionFromKing{offsetPosition - m_chessPos};
                constexpr Vec2i left{-1, 0}, right{1, 0};
                if(directionFromKing == left && 
                   b.hasCastleRights(isWhite ? CastleRights::WLONG : CastleRights::BLONG))
                {
                    Vec2i const twoToLeft   { offsetPosition + left };
                    Vec2i const threeToLeft { offsetPosition + left * 2 };
                    if( ! b.getPieceAt(twoToLeft) && ! b.getPieceAt(threeToLeft) )
                        m_pseudoLegals.emplace_back(m_chessPos, twoToLeft, CASTLE);
                }
                else if(directionFromKing == right &&
                        b.hasCastleRights(isWhite ? CastleRights::WSHORT : CastleRights::BSHORT))
                {
                    Vec2i const twoToRight{offsetPosition + right};
                    if(!b.getPieceAt(twoToRight))
                        m_pseudoLegals.emplace_back(m_chessPos, twoToRight, CASTLE);
                }
            }
            else if(piece->getSide() != m_side)//there is a piece at offset pos of the oposite color
            {
                m_pseudoLegals.emplace_back(m_chessPos, offsetPosition, KING_MOVE);
                m_attackedSquares.push_back(offsetPosition);
            }
            else m_attackedSquares.push_back(offsetPosition);
        }
    }
}

//this function assumes Board::m_checkState is equal to SINGLE_CHECK.
//called by the concrete implementations of virtual void updateLegalMoves()=0; (except for kings)
bool Piece::doesNonKingMoveResolveCheck(ChessMove const& moveToCheck, Vec2i posOfCheckingPiece, Board const& b)
{
    using enum PieceTypes;
    auto const checkingPiece = b.getPieceAt(posOfCheckingPiece);

    //if we are in check and there is an en passant move
    //available that means we are in check from a double pushed pawn.
    if(moveToCheck.moveType == ChessMove::MoveTypes::ENPASSANT)
        return true;

    //if a knight or a pawn is the piece checking the king
    //then the only (non king) move that could resolve it is a capture of the piece
    if(checkingPiece->m_type == KNIGHT || checkingPiece->m_type == PAWN)
        return(moveToCheck.dest == checkingPiece->m_chessPos);

    Vec2i const kingPos = b.getWhosTurnItIs() == Side::WHITE ?
        King::getWhiteKingPos() : King::getBlackKingPos();

    //direction from the king location to the piece putting it in check
    Vec2i direction{posOfCheckingPiece - kingPos};
    direction.x = direction.x == 0 ? 0 : direction.x/std::abs(direction.x);//extra checks for integer div by 0
    direction.y = direction.y == 0 ? 0 : direction.y/std::abs(direction.y);

    for(auto offsetPos{kingPos + direction}; ; offsetPos += direction)
    {
        if(moveToCheck.dest == offsetPos)
            return true;

        //we have reached the checking piece
        if(b.getPieceAt(offsetPos) == checkingPiece)
            break;
    }

    return false;
}

//called by Pawn::updateLegalMoves() to handle an edge case where an 
//en passant capture would put *this' king into check when *this' king
//is on the same rank (4th or 5th) as the pawns. When the capture occurs
//there might be a rook or a queen waiting on the other side.
//an example (on the 4th or 5th rank) where Q is an enemy queen, K is *this' king, 
//P is *this, and D is the double pushed enemy pawn below...
// |Q| |D|P| |K| | |  as you can see if this (P) took en passant then the double pushed pawn would be gone
//and this (P) would be on the en passant square and the queen could take the king next move
bool Pawn::doesEnPassantLeaveKingInCheck(Vec2i const enPassantMove, Board const& b) const
{
    Vec2i const kingPos
    {
        m_side == Side::WHITE ? King::getWhiteKingPos() : King::getBlackKingPos()
    };

    //just leave if the king isnt even on the same rank as the pawns
    int const whichRank = m_side == Side::WHITE ? 4 : 3;
    if(kingPos.y != whichRank)
        return false;
        
    int xDirection = enPassantMove.x - kingPos.x;
    xDirection /= std::abs(xDirection);//either -1 or 1 in the direction of the pawns D and P

    //start one square past the king in the x direction and continue until we reach
    //either the panws D and P or something potentially in the way
    Vec2i offsetPos{kingPos.x + xDirection, kingPos.y};
    for(;; offsetPos.x += xDirection)
    {
        auto const p = b.getPieceAt(offsetPos);

        if(p)
        {
            //if there is something in between the king and the pawns 
            if(offsetPos.x != m_chessPos.x && offsetPos.x != enPassantMove.x)
            {
                return false;
            }
            else break;//if the piece we reached was one of the pawns D or P       
        }
    }

    //we have reached D or P. now step over the pawns and 
    //continue along until a queen/rook is reached or the end of the board
    offsetPos.x += xDirection * 2;
    for( ; Board::isValidChessPosition(offsetPos); offsetPos.x += xDirection)
    {
        auto const p = b.getPieceAt(offsetPos);

        //if we ran into a piece check if it is a queen or a rook
        if(p)
        {
            using enum PieceTypes;
            auto const type = getType(*p);
            return type == ROOK || type == QUEEN;
        }
    }

    return false;
}

//after the pieces pseudo legal moves pinned pieces and the board check 
//state have been updated then this method can be used
void Pawn::updateLegalMoves(Board const& b)
{
    m_legalMoves.clear();
    using enum Board::CheckType;
    auto const cs = b.getCheckState();

    if(cs == DOUBLE_CHECK)
        return;

    if(isPiecePinned())
    {
        if(cs == SINGLE_CHECK)
            return;

        //see if any moves available to us are a
        //capture of the piece pinning us to our king
        for(auto const& move : m_pseudoLegals)
        {
            if(areSquaresOnSameDiagonal(m_chessPos, m_locationOfPiecePinningThis))
            {
                if(m_locationOfPiecePinningThis == move.dest)//if we are capturing the pinning piece
                {
                    m_legalMoves.push_back(move);
                }
                else//if the move isnt capturing the pinning piece
                {
                    //and the move is an en passant on the same diagonal as the pinning piece
                    if(move.moveType == ChessMove::MoveTypes::ENPASSANT &&
                        areSquaresOnSameDiagonal(move.dest, m_locationOfPiecePinningThis))
                    {
                        m_legalMoves.push_back(move);
                    }
                }
            }   
            else//if the pinning piece is on the same rank/file
            {
                if(areSquaresOnSameRankOrFile(move.dest, m_chessPos))
                    m_legalMoves.push_back(move);
            }
        }
    }
    else//*this is not pinned to its king
    {
        if(cs == SINGLE_CHECK)
        {
            for(auto const& move : m_pseudoLegals)
            {
                if(doesNonKingMoveResolveCheck(move, b.getLocationOfCheckingPiece(), b))
                    m_legalMoves.push_back(move);
            }
        }
        else//if the check state is equal to NO_CHECK and we are not pinned
        {
            //dont do a search for the en passant move if 
            //there isnt even an en passant target available
            if( ! b.isEnPassantAvailable() )
            {            
                //at this point we dont need pseudoLegals until next chess move
                //at which point we will push_back more moves into it again. 
                //So we might as well do a move assignment instead of a copy assignment here
                m_legalMoves = std::move(m_pseudoLegals);
            }
            else//if there was a pawn double push last move
            {
                for(auto const& move : m_pseudoLegals)
                {
                    //a special check if the en passant capture would leave the king in check
                    //when the king is on the same rank as the pawns
                    if(move.moveType == ChessMove::MoveTypes::ENPASSANT && doesEnPassantLeaveKingInCheck(move.dest, b))
                        continue;

                    m_legalMoves.push_back(move);
                }
            }
        }
    }
}

//after the pieces pseudo legal moves pinned pieces and the board check 
//state have been updated then this method can be used
void Knight::updateLegalMoves(Board const& b)
{
    using enum Board::CheckType;
    auto const cs = b.getCheckState();
    m_legalMoves.clear();

    //if a knight is pinned to the king or the king is in 
    //a double check then the knight will never have any legal moves
    if(isPiecePinned() || cs == DOUBLE_CHECK)
        return;

    if(cs == SINGLE_CHECK)
    {
        for(auto const& move : m_pseudoLegals)
        {
            if(doesNonKingMoveResolveCheck(move, b.getLocationOfCheckingPiece(), b))
                m_legalMoves.push_back(move);
        }
    }
    else 
    {
        //at this point we dont need pseudoLegals until next chess move
        //at which point we will push_back more moves into it again. 
        //So we might as well do a move assignment instead of a copy assignment here
        m_legalMoves = std::move(m_pseudoLegals);
    }
}

//after the pieces pseudo legal moves pinned pieces and the board check 
//state have been updated then this method can be used
void Rook::updateLegalMoves(Board const& b)
{
    using enum Board::CheckType;
    auto const cs = b.getCheckState(); 
    m_legalMoves.clear();

    if(cs == DOUBLE_CHECK)
        return;

    if(isPiecePinned())
    {
        //if the king is in check and the rook is pinned there are no legal moves
        if(cs == SINGLE_CHECK) 
            return;

        //if the rook is pinned and its on the same diagonal as the king then there wont be any legal moves
        if(areSquaresOnSameDiagonal(m_locationOfPiecePinningThis, m_chessPos))
            return;

        Vec2i const direction2PinningPiece{m_locationOfPiecePinningThis - m_chessPos};

        for(auto const& move : m_pseudoLegals)
        {
            Vec2i direction2Move{move.dest - m_chessPos};

            int intDotProduct = direction2Move.x * direction2PinningPiece.x + 
                                direction2Move.y * direction2PinningPiece.y;

            if(intDotProduct != 0)
                m_legalMoves.push_back(move);
        }
    }
    else//if we are not pinned to the king of the same color
    {
        if(cs == SINGLE_CHECK)
        {
            for(auto const& move : m_pseudoLegals)
            {
                if(Piece::doesNonKingMoveResolveCheck(move, b.getLocationOfCheckingPiece(), b))
                    m_legalMoves.push_back(move);
            }
        }
        else m_legalMoves = m_pseudoLegals;//if check state == NO_CHECK
    }
}

//after the pieces pseudo legal moves pinned pieces and the board check 
//state have been updated then this method can be used
void Bishop::updateLegalMoves(Board const& b)
{
    m_legalMoves.clear();
    using enum Board::CheckType;
    auto const cs = b.getCheckState();
    bool const isThisPinned = isPiecePinned();

    if(cs == DOUBLE_CHECK)
        return;

    if(isThisPinned)
    {
        if(cs == SINGLE_CHECK)
            return;

        //if the bishop is positioned orthogonally to its king and 
        //its pinned then it wont have any legal moves
        if(areSquaresOnSameRankOrFile(m_locationOfPiecePinningThis, m_chessPos))
            return;

        Vec2i direction2pinnedPiece{m_locationOfPiecePinningThis - m_chessPos};

        //filter through the pseudo legal moves so the only moves that make it 
        //into the fully legal moves are the ones that are on on the same diagonal as the bishop 
        //and the kingand the piece pinning the bisop to the king
        for(auto const& move : m_pseudoLegals)
        {
            Vec2i direction2PsedoLegalMove{move.dest - m_chessPos};
            int intDotProduct = direction2pinnedPiece.x * direction2PsedoLegalMove.x +
                                direction2pinnedPiece.y * direction2PsedoLegalMove.y;

            //since all the vectors compared are orthogonal or parallel (because they are all bisop moves being 
            //compared to other bishop moves) to each other the only results for their dot product will be 
            //negative parallel, positive parallel or 0. therefore if the dot product is 
            //not 0 then the pseudo legal move is on the same diagonal as the direction to the pinning piece
            if(intDotProduct != 0)
                m_legalMoves.push_back(move);
        }
    }
    else//if we are not pinnined to the king
    {
        if(cs == SINGLE_CHECK)
        {
            for(auto const& move : m_pseudoLegals)
            {
                if(doesNonKingMoveResolveCheck(move, b.getLocationOfCheckingPiece(), b))
                    m_legalMoves.push_back(move);                
            }
        }
        else
        {
            //at this point we dont need pseudoLegals until next chess move
            //at which point we will push_back more moves into it again. 
            //So we might as well do a move assignment instead of a copy assignment here
            m_legalMoves = std::move(m_pseudoLegals);
        }
    }
}

//after the pieces pseudo legal moves pinned pieces and the board check 
//state have been updated then this method can be used
void Queen::updateLegalMoves(Board const& b)
{
    m_legalMoves.clear();
    using enum Board::CheckType;
    auto const cs = b.getCheckState();
    bool isThisPinned = isPiecePinned();

    if(cs == DOUBLE_CHECK)
        return;

    if(isThisPinned)
    {
        if(cs == SINGLE_CHECK) return;
        
        for(auto const& move : m_pseudoLegals)
        {
            bool const isMoveOnSameDiagonalAsThis = 
                areSquaresOnSameDiagonal(move.dest, m_chessPos);

            bool const isThisOnSameDiagonalAsPinningPiece = 
                areSquaresOnSameDiagonal(m_locationOfPiecePinningThis, m_chessPos);

            if(isMoveOnSameDiagonalAsThis != isThisOnSameDiagonalAsPinningPiece) 
                continue;

            Vec2i const direction2Move{move.dest - m_chessPos};
            Vec2i const direction2PinningPiece{m_locationOfPiecePinningThis - m_chessPos};

            int const intDotProduct = direction2Move.x * direction2PinningPiece.x +
                                      direction2Move.y * direction2PinningPiece.y;

            if(intDotProduct != 0)
                m_legalMoves.push_back(move);
        }
    }
    else
    {
        if(cs == SINGLE_CHECK)
        {
            for(auto const& move : m_pseudoLegals)
            {
                if(doesNonKingMoveResolveCheck(move, b.getLocationOfCheckingPiece(), b))
                    m_legalMoves.push_back(move);
            }
        }
        else
        {
            //at this point we dont need pseudoLegals until next chess move
            //at which point we will push_back more moves into it again. 
            //So we might as well do a move assignment instead of a copy assignment here
            m_legalMoves = std::move(m_pseudoLegals);
        }
    }
}

//after the pieces pseudo legal moves pinned pieces and the board check 
//state have been updated then this method can be used
void King::updateLegalMoves(Board const& b)
{
    m_legalMoves.clear();
    bool const isWhite = m_side == Side::WHITE;

    //get a vector of all the attacked squares by the opposite side
    std::vector<Vec2i> const attackedSquares
    {
        b.getAttackedSquares(isWhite ? Side::BLACK : Side::WHITE)
    };
    auto const cend = attackedSquares.cend(), cbegin = attackedSquares.cbegin();

    bool const hasShortCastleRights =
        b.hasCastleRights(isWhite ? CastleRights::WSHORT : CastleRights::BSHORT);
    bool const hasLongCastleRights  =
        b.hasCastleRights(isWhite ? CastleRights::WLONG  : CastleRights::BLONG);

    bool shouldEraseShortCastle = false, shouldEraseLongCastle = false;

    //filter out the pseudo legal moves that are in the list of attacked squares
    for(auto const& move : m_pseudoLegals)
    {   
        //direction is from whites perspective
        Vec2i const king2Move{move.dest - m_chessPos};
        constexpr Vec2i left{-1, 0}, right{1, 0};

        if(std::find(cbegin, cend, move.dest) != cend)
        {
            if(king2Move == left && hasLongCastleRights) 
                shouldEraseLongCastle = true;
            else if(king2Move == right && hasShortCastleRights) 
                shouldEraseShortCastle = true;
        }
        else m_legalMoves.push_back(move);
    }

    auto const eraseCastle = [this](bool const isLongCastle) -> void
    {
        auto const end = m_legalMoves.end(), begin = m_legalMoves.begin();
        auto const castleMove = std::find_if(begin, end,
        [this, isLongCastle](auto const& move) -> bool
        {
            Vec2i const castlePos{m_chessPos.x + 
                (isLongCastle ? -2 : 2), m_chessPos.y};
            return move.dest == castlePos;
        });

        if(castleMove != end) m_legalMoves.erase(castleMove);
    };

    auto const cs = b.getCheckState();
    if(shouldEraseShortCastle || cs == Board::CheckType::SINGLE_CHECK) eraseCastle(false);
    if(shouldEraseLongCastle  || cs == Board::CheckType::SINGLE_CHECK) eraseCastle(true);
}

bool Piece::areSquaresOnSameDiagonal(Vec2i const pos0, Vec2i const pos1)
{
    int const xDistance = std::abs(pos0.x - pos1.x);
    int const yDistance = std::abs(pos0.y - pos1.y);
    return (yDistance == xDistance);
}

bool Piece::areSquaresOnSameRankOrFile(Vec2i const square0, Vec2i const square1)
{
    return square0.x == square1.x || //if squares are on same file or..
           square0.y == square1.y;   //if squares are on same rank
}

//updates *this's internal m_squareOfPiecePinningThis variable to hold
//the location of the square pinning *this. If no piece is pinning
//*this to its king then m_squareOfPiecePinningThis will be INVALID_VEC2I (-1, -1)
void Piece::updatePinnedInfo(Board const& b)
{
    if(m_type == PieceTypes::KING)
        return;

    resetLocationOfPiecePinningThis();//reset m_locationOfPiecePinningThis back to INVALID_VEC2I (-1, -1)
    Vec2i const kingPos = m_side == Side::WHITE ?
        King::getWhiteKingPos() : King::getBlackKingPos();

    bool isDiagonal = false;
    if(areSquaresOnSameDiagonal(kingPos, m_chessPos)) isDiagonal = true;//if the king is on the same diagonal as *this
    else if(!areSquaresOnSameRankOrFile(kingPos, m_chessPos)) return;//if we arent on the same diagonal or rank/file 

    Vec2i const kingToThis = m_chessPos - kingPos;//direction from the king to *this
    Vec2i const direction//a vector for which direction to look to see if *this is pinned by another piece
    {
        (kingToThis.x == 0) ? 0 : kingToThis.x/std::abs(kingToThis.x),//conditional checks to make sure no divide by 0 happens
        (kingToThis.y == 0) ? 0 : kingToThis.y/std::abs(kingToThis.y)
    };
    Vec2i offsetPosition = kingPos;//start at the king

    for(;;)//go from king to *this to ensure that there isnt anything inbetween *this and the king
    {
        offsetPosition += direction;//make a step...

        assert(Board::isValidChessPosition(offsetPosition));

        auto const p = b.getPieceAt(offsetPosition);
        if(p)//if there is a piece at offsetPosition
        {
            if(p.get() != this)//if there is a piece in between *this and it's king
            {
                return;
            }
            else//if *p is *this
            {
                offsetPosition += direction;//step over *this and leave the loop
                break;
            }
        }
    }

    //continue walking along from where the first loop left off (the first loop left off 1 square past *this)
    //until we potentially find a piece that would be pinning *this to the king.
    //I could have merged this into the above loop. This seemed more readable though
    for( ; Board::isValidChessPosition(offsetPosition); offsetPosition += direction)
    {
        auto const p = b.getPieceAt(offsetPosition);
        using enum PieceTypes;
        if(p)//if there is a piece at offsetPos
        {
            if(p->m_side == m_side)//if the piece we ran into is the same color as *this
                return;

            if(isDiagonal)//if *this is on the same diagonal as the king
            {
                m_locationOfPiecePinningThis = p->m_type == QUEEN || p->m_type == BISHOP
                    ? p->m_chessPos : INVALID_VEC2I;
                return;
            }
            else//if *this is on the same rank or file as the king
            {
                m_locationOfPiecePinningThis = p->m_type == QUEEN || p->m_type == ROOK
                    ? p->m_chessPos : INVALID_VEC2I;
                return;
            }
        }
    }
}

void Piece::setChessPosition(Vec2i const newChessPos)
{   
    m_chessPos = newChessPos;

    //update the kings static position members
    if(m_type == PieceTypes::KING)
    {
        if(m_side == Side::WHITE) King::setWhiteKingPos(newChessPos);
        else King::setBlackKingPos(newChessPos);
    }
}