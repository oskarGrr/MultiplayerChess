#include "SoundManager.hpp"
#include "ChessEvents.hpp"
#include <cassert>

SoundManager::WavSound::WavSound(std::filesystem::path const& filePath) 
    : m_spec{}, m_audioBuffer{nullptr}, m_deviceID{}, m_audioLength{0u}
{
    if( ! SDL_LoadWAV(filePath.string().c_str(), &m_spec, &m_audioBuffer, &m_audioLength) )
        throw std::exception(SDL_GetError());

    if((m_deviceID = SDL_OpenAudioDevice(nullptr, 0, &m_spec, nullptr, 0)) == 0)
        throw std::exception(SDL_GetError());
}

SoundManager::WavSound::~WavSound()
{
    SDL_FreeWAV(m_audioBuffer);
    SDL_CloseAudioDevice(m_deviceID);
}

void SoundManager::WavSound::playFullSound() const
{
    SDL_QueueAudio(m_deviceID, m_audioBuffer, m_audioLength);
    SDL_PauseAudioDevice(m_deviceID, 0);
}

void SoundManager::WavSound::stopSound() const
{
    SDL_PauseAudioDevice(m_deviceID, 1);
}

SoundManager::SoundManager(BoardEventSystem::Subscriber& boardEventSubscriber)
    : mBoardEventSubscriber{boardEventSubscriber}
{
    mChessMoveCompletedEventID = mBoardEventSubscriber.sub<BoardEvents::MoveCompleted>(
    [this](Event const& e)
    {
        auto const& evnt { e.unpack<BoardEvents::MoveCompleted>() };
        playCorrectMoveAudio(evnt.move);
    });
}

SoundManager::~SoundManager()
{
    mBoardEventSubscriber.unsub<BoardEvents::MoveCompleted>(mChessMoveCompletedEventID);
}

//to be called in postMoveUpdate() after the correct above single ha
//method was called and before m_lastCapturedPiece is reset to null
void SoundManager::playCorrectMoveAudio(ChessMove const& move)
{
    if(move.wasCapture) { mCaptureMoveSound.playFullSound(); }
    else if(move.moveType == ChessMove::MoveTypes::CASTLE) { mCastleMoveSound.playFullSound(); }
    else { mNormalMoveSound.playFullSound(); }
}