#pragma once
#include "SDL.h"
#include "Vector2i.hpp"
#include <unordered_map>
#include <type_traits>
#include <string_view>
#include <memory>

class TextureManager
{
public:
    TextureManager(SDL_Renderer* renderer);

    class Texture
    {
    public:
        Texture(std::string_view filePath, SDL_Renderer* renderer);
        Texture(SDL_Texture* texture);
        
        Vec2i getSize() const {return mSize;}
        auto getTexture() const {return mTexturePtr;}

        Texture& operator=(Texture const&)=delete;
        Texture& operator=(Texture&&)=delete;
        Texture(Texture const&)=delete;
        Texture(Texture&&)=delete;

    private:
        std::shared_ptr<SDL_Texture> mTexturePtr;
        Vec2i mSize{};
    };

    enum struct WhichTexture
    {
        INVALID = -1,

        BOARD_TEXTURE,

        BLACK_QUEEN,
        BLACK_KING,
        BLACK_KNIGHT,
        BLACK_ROOK,
        BLACK_PAWN,
        BLACK_BISHOP,

        WHITE_QUEEN,
        WHITE_KING,
        WHITE_KNIGHT,
        WHITE_ROOK,
        WHITE_PAWN,
        WHITE_BISHOP,

        GRAY_CIRCLE,
        RED_CIRCLE
    };

    Texture const& getTexture(WhichTexture) const;

private:
    std::unordered_map<WhichTexture, Texture> mTextures;

    static void initCircleTexture(int radius, Uint8 RR, Uint8 GG, 
        Uint8 BB, Uint8 AA, SDL_Texture** toInit, SDL_Renderer* renderer);
};