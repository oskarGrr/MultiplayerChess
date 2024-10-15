#include "TextureManager.hpp"
#include "ChessRenderer.hpp"
#include "SDL_image.h"
#include "SDL.h"
#include "errorLogger.h"

TextureManager::TextureManager(SDL_Renderer* renderer)
{
    //Load the chess piece textures.
    mTextures.try_emplace(WhichTexture::BLACK_QUEEN,   "textures/bQueen.png",  renderer);
    mTextures.try_emplace(WhichTexture::BLACK_KING,    "textures/bKing.png",   renderer);
    mTextures.try_emplace(WhichTexture::BLACK_KNIGHT,  "textures/bKnight.png", renderer);
    mTextures.try_emplace(WhichTexture::BLACK_ROOK,    "textures/bRook.png",   renderer);
    mTextures.try_emplace(WhichTexture::BLACK_PAWN,    "textures/bPawn.png",   renderer);
    mTextures.try_emplace(WhichTexture::BLACK_BISHOP,  "textures/bBishop.png", renderer);
    mTextures.try_emplace(WhichTexture::WHITE_QUEEN,   "textures/wQueen.png",  renderer);
    mTextures.try_emplace(WhichTexture::WHITE_KING,    "textures/wKing.png",   renderer);
    mTextures.try_emplace(WhichTexture::WHITE_KNIGHT,  "textures/wKnight.png", renderer);
    mTextures.try_emplace(WhichTexture::WHITE_ROOK,    "textures/wRook.png",   renderer);
    mTextures.try_emplace(WhichTexture::WHITE_PAWN,    "textures/wPawn.png",   renderer);
    mTextures.try_emplace(WhichTexture::WHITE_BISHOP,  "textures/wBishop.png", renderer);

    int const radius { static_cast<int>(ChessRenderer::getSquareSize() / 6) };

    //Initialize the gray circle texture.
    SDL_Texture* greyCircleTex {nullptr};
    initCircleTexture(radius, 0x6F, 0x6F, 0x6F, 0x9F, &greyCircleTex, renderer);
    mTextures.try_emplace(WhichTexture::GRAY_CIRCLE, greyCircleTex);

    //Initialize the red circle texture.
    SDL_Texture* redCircleTex {nullptr};
    initCircleTexture(radius, 0xDE, 0x31, 0x63, 0x7F, &redCircleTex, renderer);
    mTextures.try_emplace(WhichTexture::RED_CIRCLE, redCircleTex);
}

TextureManager::Texture::Texture(std::string_view filePath, SDL_Renderer* renderer)
{
    SDL_Texture* tex {IMG_LoadTexture(renderer, filePath.data())};
    mTexturePtr = { tex, [](SDL_Texture* t){SDL_DestroyTexture(t);} };
    SDL_QueryTexture(mTexturePtr.get(), nullptr, nullptr, &mSize.x, &mSize.y);
}

TextureManager::Texture::Texture(SDL_Texture* tex)
    : mTexturePtr{ tex, [](SDL_Texture* t){SDL_DestroyTexture(t);} }
{
    SDL_QueryTexture(mTexturePtr.get(), nullptr, nullptr, &mSize.x, &mSize.y);
}

auto TextureManager::getTexture(WhichTexture whichTexture) 
    const -> Texture const&
{
    auto it {mTextures.find(whichTexture)};
    assert(it != mTextures.end());
    return it->second;
}

//generates a circle texture at startup to use later
void TextureManager::initCircleTexture(int radius, Uint8 RR, Uint8 GG, Uint8 BB,
    Uint8 AA, SDL_Texture** toInit, SDL_Renderer* renderer)
{
//make sure the ordering of the RGBA bytes will be the same
//regarless of endianess of the target platform
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 rMask = 0xFF000000;
    Uint32 gMask = 0x00FF0000;
    Uint32 bMask = 0x0000FF00;
    Uint32 aMask = 0x000000FF;
    Uint32 circleColor = (RR << 24) + (GG << 16) + (BB << 8) + AA;
#else
    uint32_t rMask = 0x000000FF;
    uint32_t gMask = 0x0000FF00;
    uint32_t bMask = 0x00FF0000;
    uint32_t aMask = 0xFF000000;
    uint32_t circleColor = (AA << 24) + (BB << 16) + (GG << 8) + RR;
#endif
    auto const diameter { radius * 2 };
    SDL_Rect const boundingBox{-radius, -radius, diameter, diameter};
    uint32_t const pixelCount = diameter * diameter;
    auto pixels = std::make_unique<uint32_t[]>(pixelCount);

    auto radiusSquared { radius * radius };
    int xOffset = -radius, yOffset = -radius;
    for(int x = 0; x < diameter; ++x)
    {
        for(int y = 0; y < diameter; ++y)
        {
            if((x-radius)*(x-radius) + (y-radius)*(y-radius) <= radiusSquared)
            {
                pixels[x + y * diameter] = circleColor;
            }
        }
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom
    (
         pixels.get(), 
         diameter, 
         diameter,
         32, 
         diameter * sizeof(uint32_t), 
         rMask, gMask, bMask, aMask
    );
   
    if(!surface) { FileErrorLogger::get().log(SDL_GetError()); }

    *toInit = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
}