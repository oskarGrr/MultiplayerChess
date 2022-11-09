#pragma once
#include "SDL.h"

//very simple wrapper for an SDL wave sound 
class WavSound
{
public:
    
    WavSound()=delete;
    WavSound(char const* filePath);
    ~WavSound();
    void playFullSound();
    void stopSound();

private:

    SDL_AudioSpec m_spec;
    Uint8* m_audioBuffer;
    SDL_AudioDeviceID m_deviceID;
    Uint32 m_audioLength;
};