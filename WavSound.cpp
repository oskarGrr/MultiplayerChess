#include "WavSound.h"

WavSound::WavSound(char const* filePath) 
    : m_spec{}, m_audioBuffer(nullptr), m_deviceID{}, m_audioLength(0u)
{
    if(!SDL_LoadWAV(filePath, &m_spec, &m_audioBuffer, &m_audioLength)) throw SDL_GetError();
    m_deviceID = SDL_OpenAudioDevice(nullptr, 0, &m_spec, nullptr, 0);
    if(m_deviceID == 0) throw SDL_GetError();
}

WavSound::~WavSound()
{
    SDL_FreeWAV(m_audioBuffer);
    SDL_CloseAudioDevice(m_deviceID);
}

void WavSound::playFullSound()
{
    SDL_QueueAudio(m_deviceID, m_audioBuffer, m_audioLength);
    SDL_PauseAudioDevice(m_deviceID, 0);
}

void WavSound::stopSound()
{
    SDL_PauseAudioDevice(m_deviceID, 1);
}