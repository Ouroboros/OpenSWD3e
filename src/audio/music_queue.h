#pragma once

#include <array>
#include <string>
#include <string_view>

#include "core/types.h"

namespace swd3e
{

class MidiPlayer;
class StreamPlayer;

// A queued piece of music (0x14 bytes: the name, the id it plays as, its type and two words nothing reads).
struct MusicSlot
{
    static constexpr int Stream = 1;  // `type`: a .wav or .mp3 file
    static constexpr int Midi = 2;    // an .xmi file

    std::u8string name;  // +0x00 (none for a file of another type)
    int id = 0;          // +0x04
    int type = 0;        // +0x08
    i32 unk0C = 0;       // +0x0C
    i32 unk10 = 0;       // +0x10
};

// The music of the game (swd3e; the object at 0x00578A70): three playlists of two pieces each (the map's, the
// story's and the battle's) and the one playing; a switch fades the playing piece out and the next update, once it
// has stopped, starts the new list.
class MusicQueue
{
public:
    // The lists, and the modes playing them.
    enum List : int
    {
        NoList = 0,
        FirstList = 1,   // the scene's tunes (the map, the title)
        SecondList = 2,  // the story's tunes
        ThirdList = 3,   // the battle's tune
        Silence = 4,     // a pending mode: none plays after the current piece
    };

    static constexpr std::size_t Pieces = 2;
    static constexpr i32 MaxVolume = 0x7F;
    static constexpr int PlayOnce = 1;
    // The ways a list repeats (Switch's `repeat`).
    static constexpr int RepeatList = 1;  // the two pieces alternate
    static constexpr int OnceThrough = 2;

    MusicQueue(StreamPlayer& streams, MidiPlayer& midi);
    ~MusicQueue();

    static int TypeOf(std::u8string_view name);
    int SetSlot(std::u8string_view name, int id, std::size_t index, int list);
    int ClearSlots(int list);
    void SetVolume(i32 volume);
    int Switch(int list, int repeat, int fadeSteps);
    int Update();
    bool IsStopped();
    int Mode() const;

private:
    std::array<MusicSlot, Pieces>* Slots(int list);

    StreamPlayer& streams;
    MidiPlayer& midi;
    // +0x00..+0x08: three words (0x1E, 0, 0) the constructor sets and nothing reads
    i32 mode = Silence;                   // +0x0C the list playing
    i32 pending = Silence;                // +0x10 the list to switch to
    i32 index3 = 0;                       // +0x14 the next piece of each list and how it repeats
    i32 repeat3 = 0;                      // +0x18
    i32 index2 = 0;                       // +0x1C
    i32 repeat2 = 0;                      // +0x20
    i32 index1 = 0;                       // +0x24
    i32 repeat1 = 0;                      // +0x28
    i32 volume = MaxVolume;               // +0x2C
    MusicSlot current;                    // +0x430
    std::array<MusicSlot, Pieces> list3;  // +0x444
    std::array<MusicSlot, Pieces> list2;  // +0x46C
    std::array<MusicSlot, Pieces> list1;  // +0x494
};

}  // namespace swd3e
