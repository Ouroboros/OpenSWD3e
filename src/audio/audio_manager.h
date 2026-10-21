#pragma once

#include <string>

#include "audio/music_queue.h"
#include "audio/sequence_player.h"
#include "audio/sound_effects.h"
#include "core/types.h"

namespace swd3e
{

class AudioDevice;

// Miles startup (the object at 0x004A9AC8).
class AudioSystem
{
public:
    AudioSystem();
    ~AudioSystem();

    void Startup(const std::u8string& redistDirectory);
    void Shutdown();

private:
    bool started = false;
    std::u8string redistDirectory;
};

// The game's audio: Miles, the MIDI and stream players, the sound effects, the music queue, the sound wrappers the game
// calls (volumes 0..11 of the options scaled to 0..128) and the fade the talk commands give the music.
class AudioManager
{
public:
    explicit AudioManager(AudioDevice& device);

    AudioSystem& System();
    MidiPlayer& Midi();
    StreamPlayer& Streams();
    SoundEffects& Effects();
    MusicQueue& Queue();

    int PlaySound(u16 id, int volume);
    int StopSound(u16 id);
    int StopAllSounds();
    int SetSoundPan(u16 id, int pan);
    void Update();
    void SetFadeMode(int mode);
    void SetFadeLength(int length);
    int FadeMode() const;
    int FadeLength() const;
    // The rebuild's smooth motion: no sound is played while a menu frame is painted again between frames.
    void SetMuted(bool value);

private:
    AudioSystem system;
    MidiPlayer midi;
    StreamPlayer streams;
    SoundEffects effects;
    MusicQueue queue;
    int musicFadeMode = 0;    // 0x004D0B8C talk command MusicFade (a new game sets 1)
    int musicFadeLength = 0;  // 0x004D0E58 the updates the next switch of the music fades over
    bool muted = false;
};

}  // namespace swd3e
