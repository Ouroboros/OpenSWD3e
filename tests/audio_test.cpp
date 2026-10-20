#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

#include "audio/audio_manager.h"
#include "config/config.h"
#include "core/types.h"
#include "game_data.h"
#include "platform/audio_decoder.h"
#include "platform/audio_device.h"
#include "test.h"

using swd3e::i16;
using swd3e::u8;

namespace
{

// The dummy driver plays nothing and needs no sound hardware.
void UseDummyAudio()
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
}

constexpr int kLoud = 200;
constexpr int kQuiet = 3;
constexpr int kFarLeft = -70;  // pans run from -CenterPan to the right of the centre
constexpr int kFarRight = 100;
constexpr int kFadedVolume = 1600;
constexpr int kFadeSteps = 3;
constexpr int kTooManySteps = 5000;  // more steps than the volume: the step stays 1
constexpr std::size_t kFormAt = 8;   // a rebuilt RIFF image: "WAVEfmt " after the RIFF header, "data" after the format
constexpr std::size_t kDataAt = 0x24;
constexpr int kMixFrames = 256;
constexpr int kMusicVolume = 0x60;
constexpr int kFadeFrames = 3;
constexpr int kMidiVolume = 100;

}  // namespace

TEST_CASE("audio: clamps and fade steps")
{
    using swd3e::SequenceNode;
    CHECK_EQ(swd3e::ClampVolume(kLoud), SequenceNode::MaxLevel);
    CHECK_EQ(swd3e::ClampVolume(-kQuiet), 0);
    CHECK_EQ(swd3e::ClampPan(0), SequenceNode::CenterPan);
    CHECK_EQ(swd3e::ClampPan(kFarLeft), 0);
    CHECK_EQ(swd3e::ClampPan(kFarRight), SequenceNode::MaxLevel);
    SequenceNode node;
    node.volume = kFadedVolume;
    CHECK_EQ(swd3e::SetFadeStep(node, kFadeSteps), kFadedVolume / kFadeSteps);
    CHECK_EQ(node.fadeStep, kFadedVolume / kFadeSteps);
    CHECK_EQ(swd3e::SetFadeStep(node, kTooManySteps), 0);
    CHECK_EQ(node.fadeStep, 1);
}

TEST_CASE("audio: all.snd index, RIFF rebuild and sample bookkeeping")
{
    if (swd3etest::ReadGameFile("all.snd").empty())
    {
        std::printf("  (all.snd not found, skipped)\n");
        return;
    }

    UseDummyAudio();
    swd3e::AudioDevice device;
    swd3e::AudioManager audio(device);
    swd3e::SoundEffects& effects = audio.Effects();
    CHECK(effects.Open(swd3etest::GameDirectory()));
    CHECK(effects.IsOpen());
    CHECK(effects.HasDriver());
    int id = 0;

    for (int candidate = 1; candidate <= swd3e::SoundEffects::Sounds && id == 0; ++candidate)
    {
        if (effects.LoadSound(candidate) != nullptr)
        {
            id = candidate;
        }
    }

    CHECK(id != 0);
    const auto image = effects.LoadSound(id);

    auto holds = [&image](std::size_t at, std::string_view tag)
    {
        return std::memcmp(image->data() + at, tag.data(), tag.size()) == 0;
    };

    CHECK(holds(0, "RIFF"));
    CHECK(holds(kFormAt, "WAVEfmt "));
    CHECK(holds(kDataAt, "data"));
    CHECK_EQ(audio.PlaySound(static_cast<swd3e::u16>(id), swd3e::Config::MaxVolume), 0);
    CHECK(effects.Find(id) != nullptr);
    CHECK_EQ(effects.SetPan(id, kFarRight), swd3e::SequenceNode::MaxLevel);
    std::vector<i16> mixed;
    device.Mix(mixed, kMixFrames);
    CHECK_EQ(mixed.size(), static_cast<std::size_t>(kMixFrames) * swd3e::kMixChannels);
    CHECK_EQ(audio.StopSound(static_cast<swd3e::u16>(id)), 1);
    CHECK(effects.Find(id) == nullptr);
    CHECK(effects.Close());
}

TEST_CASE("audio: music stream plays, fades out and is retired")
{
    using swd3e::AudioManager;

    if (swd3etest::ReadGameFile("Music/Battle_Arab01.mp3").empty())
    {
        std::printf("  (music not found, skipped)\n");
        return;
    }

    UseDummyAudio();
    swd3e::AudioDevice device;
    AudioManager audio(device);
    CHECK(audio.Streams().Open());
    const std::u8string file = swd3etest::GameFile("Music/Battle_Arab01.mp3");
    CHECK_EQ(audio.Streams().Play(file, AudioManager::MusicId, kMusicVolume, 0), AudioManager::MusicId);
    CHECK(!audio.IsMusicStopped());
    CHECK_EQ(audio.Streams().Play(file, AudioManager::MusicId, kMusicVolume, 0), 0);
    CHECK_EQ(audio.SetMusicVolume(swd3e::Config::MaxVolume), swd3e::SequenceNode::MaxLevel);
    CHECK_EQ(audio.SetMusicFade(AudioManager::FadeOverLength, kFadeFrames), kFadeFrames);
    audio.UpdateMusicFade();

    for (int update = 0; update < kFadeFrames; ++update)
    {
        audio.Streams().Update();
    }

    CHECK(audio.IsMusicStopped());
    CHECK_EQ(audio.PollMusicFade(), 1);
    CHECK_EQ(audio.PollMusicFade(), 0);
}

TEST_CASE("audio: no MIDI device")
{
    swd3e::AudioDevice device;
    swd3e::MidiPlayer midi(device);
    CHECK(!midi.Open(0));
    CHECK(midi.Error() == "No MIDI output device //No MIDI output device");
    CHECK_EQ(midi.Play(u8"x.mid", 1, kMidiVolume, 1), 0);
    CHECK(midi.IsStopped(0));
}
