#include "audio/audio_manager.h"

#include "config/config.h"

namespace swd3e
{

namespace
{

constexpr int kMilesVolumeUnit = 0x80;  // Miles volumes run to 0x7F

// The options' volume 0..11 as a Miles volume.
int ScaledVolume(int volume)
{
    return volume * kMilesVolumeUnit / Config::MaxVolume;
}

}  // namespace

// swd3e.exe 0x0049CDD0 (the object at 0x00578A68, made by the global constructor 0x0049CDC0)
AudioSystem::AudioSystem() = default;

AudioSystem::~AudioSystem()
{
    this->Shutdown();
}

// swd3e.exe 0x0049CE20: the redist directory and Miles startup (a platform replacement: the device opens elsewhere)
void AudioSystem::Startup(const std::u8string& redistDirectory)
{
    this->redistDirectory = redistDirectory;
    this->started = true;
}

// swd3e.exe 0x0049CE10 (AIL_shutdown)
void AudioSystem::Shutdown()
{
    this->started = false;
}

AudioManager::AudioManager(AudioDevice& device)
    : midi(device),
      streams(device),
      effects(device),
      queue(this->streams, this->midi)
{
}

AudioSystem& AudioManager::System()
{
    return this->system;
}

MidiPlayer& AudioManager::Midi()
{
    return this->midi;
}

StreamPlayer& AudioManager::Streams()
{
    return this->streams;
}

SoundEffects& AudioManager::Effects()
{
    return this->effects;
}

MusicQueue& AudioManager::Queue()
{
    return this->queue;
}

// swd3e.exe 0x00427040: sound `id` once, centred, at the options' volume 0..11 (`muted` is the smooth menu replay of a
// frame, an enhancement in deviations.md; the original has no such check)
int AudioManager::PlaySound(u16 id, int volume)
{
    if (this->muted)
    {
        return 1;
    }

    return this->effects.Play(id, ScaledVolume(volume), 0, 1);
}

// swd3e.exe 0x00427090
int AudioManager::SetSoundPan(u16 id, int pan)
{
    return this->effects.SetPan(id, pan);
}

void AudioManager::SetMuted(bool value)
{
    this->muted = value;
}

// swd3e.exe 0x00427070
int AudioManager::StopSound(u16 id)
{
    return this->effects.Stop(id);
}

// swd3e.exe 0x00427080: every sound ends and the sounds no longer played are freed (when the effects are open and
// enabled)
int AudioManager::StopAllSounds()
{
    return this->effects.StopAll() ? 1 : 0;
}

// The body of Game::AudioPump (0x0040D140): the music queue, the streams and the sound effects (the MIDI player has
// no update)
void AudioManager::Update()
{
    this->queue.Update();
    this->streams.Update();
    this->effects.Update();
}

void AudioManager::SetFadeMode(int mode)
{
    this->musicFadeMode = mode;
}

void AudioManager::SetFadeLength(int length)
{
    this->musicFadeLength = length;
}

int AudioManager::FadeMode() const
{
    return this->musicFadeMode;
}

int AudioManager::FadeLength() const
{
    return this->musicFadeLength;
}

}  // namespace swd3e
