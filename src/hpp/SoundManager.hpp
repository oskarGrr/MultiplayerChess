#pragma once
#include <filesystem>
#include <cstdint>
#include "SDL.h"
#include "ChessEvents.hpp"

class SoundManager
{
public:
    SoundManager(BoardEventSystem&);

private:

    void playCorrectMoveAudio(ChessMove const& moveType);

    //very simple wrapper for an SDL wave sound 
    class WavSound
    {
    public:

        WavSound(std::filesystem::path const&); 
        ~WavSound();

        void playFullSound() const;
        void stopSound() const;

    private:
        SDL_AudioSpec m_spec;
        Uint8* m_audioBuffer;
        SDL_AudioDeviceID m_deviceID;
        uint32_t m_audioLength;
    public:
        WavSound(WavSound const&)=delete;
        WavSound(WavSound&&)=delete;
        WavSound& operator=(WavSound const&)=delete;
        WavSound& operator=(WavSound&&)=delete;
    };

    WavSound mNormalMoveSound  {"sounds/woodChessMove.wav"};
    WavSound mCastleMoveSound  {"sounds/woodChessCastle.wav"};
    WavSound mCaptureMoveSound {"sounds/woodCaptureMove.wav"};

public:
    SoundManager(SoundManager const&)=delete;
    SoundManager(SoundManager&&)=delete;
    SoundManager& operator=(SoundManager const&)=delete;
    SoundManager& operator=(SoundManager&&)=delete;
};