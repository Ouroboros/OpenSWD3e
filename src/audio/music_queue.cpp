#include "audio/music_queue.h"

#include "audio/sequence_player.h"

namespace swd3e
{

namespace
{

constexpr std::u8string_view kWave = u8"wav";  // 0x004B1104
constexpr std::u8string_view kMp3 = u8"mp3";   // 0x004C03C4
constexpr std::u8string_view kXmi = u8"xmi";   // 0x004C03C0

}  // namespace

// swd3e.exe 0x0049CE60: no list playing, a stop pending, the full volume, every slot empty (with the global
// constructor 0x0049CE50)
MusicQueue::MusicQueue(StreamPlayer& streams, MidiPlayer& midi)
    : streams(streams),
      midi(midi)
{
}

// swd3e.exe 0x0049CE80: the exit callback 0x0049CE70 registers, the destructor of 0x00578A70 (chunk 0x0049CF00:
// ClearSlots of lists 3, 2 and 1; the slots' names are freed with them)
MusicQueue::~MusicQueue() = default;

// swd3e.exe 0x0049D310: Stream for a name holding "wav" or "mp3", Midi for one holding "xmi", 0 otherwise (anywhere in
// the name, as strstr finds them)
int MusicQueue::TypeOf(std::u8string_view name)
{
    if (name.find(kWave) != std::u8string_view::npos || name.find(kMp3) != std::u8string_view::npos)
    {
        return MusicSlot::Stream;
    }

    return name.find(kXmi) != std::u8string_view::npos ? MusicSlot::Midi : 0;
}

std::array<MusicSlot, MusicQueue::Pieces>* MusicQueue::Slots(int list)
{
    switch (list)
    {
        case MusicQueue::FirstList:
            return &this->list1;

        case MusicQueue::SecondList:
            return &this->list2;

        case MusicQueue::ThirdList:
            return &this->list3;

        default:
            return nullptr;
    }
}

// swd3e.exe 0x0049CF20: piece `index` (0 or 1) of list `list` becomes `name` played as `id` (an id of 0 or below, or
// another index or list, is ignored); a name of no known type is not kept
int MusicQueue::SetSlot(std::u8string_view name, int id, std::size_t index, int list)
{
    std::array<MusicSlot, Pieces>* slots = this->Slots(list);

    if (id <= 0 || index >= Pieces || slots == nullptr)
    {
        return 0;
    }

    MusicSlot& slot = (*slots)[index];
    slot.name.clear();
    slot.type = MusicQueue::TypeOf(name);

    if (slot.type != 0)
    {
        slot.name = name;
    }

    slot.id = id;
    slot.unk0C = 0;
    slot.unk10 = 0;
    return 0;
}

// swd3e.exe 0x0049D000: the two pieces of list `list` freed and cleared
int MusicQueue::ClearSlots(int list)
{
    std::array<MusicSlot, Pieces>* slots = this->Slots(list);

    if (slots != nullptr)
    {
        slots->fill(MusicSlot{});
    }

    return 0;
}

// swd3e.exe 0x0049D060: the volume (0..0x7F) for the pieces to come; the MIDI master volume and the stream at the
// head of the stream player take it at once
void MusicQueue::SetVolume(i32 value)
{
    this->volume = value;

    if (this->midi.IsOpen())
    {
        this->midi.SetMasterVolume(this->volume);
    }

    if (this->streams.IsOpen())
    {
        this->streams.SetVolume(this->streams.FirstId(), this->volume);
    }
}

// swd3e.exe 0x0049D0C0: the playing piece fades out over `fadeSteps` updates (a stream; a MIDI piece is only marked,
// the MIDI player has no update) and list `list` follows it from its first piece, repeating as `repeat` says (1 or
// 2, else 1); another `list` (Silence) resets the first two lists and stops the music after the piece
int MusicQueue::Switch(int list, int repeat, int fadeSteps)
{
    if (this->current.type == MusicSlot::Stream)
    {
        this->streams.FadeOut(this->current.id, fadeSteps);
    }
    else if (this->current.type == MusicSlot::Midi)
    {
        this->midi.FadeOut(this->current.id, fadeSteps);
    }

    this->pending = list;
    const bool given = repeat == MusicQueue::RepeatList || repeat == MusicQueue::OnceThrough;

    switch (list)
    {
        case MusicQueue::FirstList:
            this->index1 = 0;
            this->repeat1 = given ? repeat : MusicQueue::RepeatList;
            break;

        case MusicQueue::SecondList:
            this->index2 = 0;
            this->repeat2 = given ? repeat : MusicQueue::RepeatList;
            break;

        case MusicQueue::ThirdList:
            this->index3 = 0;
            this->repeat3 = given ? repeat : MusicQueue::RepeatList;
            break;

        default:
            this->index2 = 0;
            this->repeat2 = MusicQueue::RepeatList;
            this->index1 = 0;
            this->repeat1 = MusicQueue::RepeatList;
            break;
    }

    return 0;
}

// swd3e.exe 0x0049D1A0: once the playing piece has stopped, a pending switch is applied (a list from its first piece,
// Silence stops) and the next piece of the list playing starts once at the queue's volume; a repeating list starts
// over after its second piece
int MusicQueue::Update()
{
    if (this->current.type == MusicSlot::Stream)
    {
        if (!this->streams.IsStopped(this->current.id))
        {
            return 0;
        }
    }
    else if (this->current.type == MusicSlot::Midi)
    {
        if (!this->midi.IsStopped(this->current.id))
        {
            return 0;
        }
    }

    this->current = MusicSlot{};
    const i32 switchTo = this->pending;

    if (switchTo >= MusicQueue::FirstList && switchTo <= MusicQueue::ThirdList)
    {
        this->mode = switchTo;
        this->pending = MusicQueue::NoList;

        if (switchTo == MusicQueue::FirstList)
        {
            this->index1 = 0;
        }
        else if (switchTo == MusicQueue::SecondList)
        {
            this->index2 = 0;
        }
        else
        {
            this->index3 = 0;
        }
    }
    else if (switchTo == MusicQueue::Silence)
    {
        this->mode = MusicQueue::NoList;
        this->pending = MusicQueue::NoList;
    }

    i32* index = nullptr;
    i32* repeat = nullptr;
    std::array<MusicSlot, Pieces>* slots = nullptr;

    switch (this->mode)
    {
        case MusicQueue::FirstList:
            index = &this->index1;
            repeat = &this->repeat1;
            slots = &this->list1;
            break;

        case MusicQueue::SecondList:
            index = &this->index2;
            repeat = &this->repeat2;
            slots = &this->list2;
            break;

        case MusicQueue::ThirdList:
            index = &this->index3;
            repeat = &this->repeat3;
            slots = &this->list3;
            break;

        default:
            return 0;
    }

    if (*index >= static_cast<i32>(MusicQueue::Pieces))
    {
        return 0;
    }

    this->current = (*slots)[static_cast<std::size_t>(*index)];

    if (!this->current.name.empty())
    {
        if (this->current.type == MusicSlot::Stream)
        {
            this->streams.Play(this->current.name, this->current.id, this->volume, MusicQueue::PlayOnce);
        }
        else if (this->current.type == MusicSlot::Midi)
        {
            this->midi.Play(this->current.name, this->current.id, this->volume, MusicQueue::PlayOnce);
        }
    }

    ++*index;

    if (*repeat == MusicQueue::RepeatList && *index >= static_cast<i32>(MusicQueue::Pieces))
    {
        *index = 0;
    }

    return 0;
}

// swd3e.exe 0x0049D370: whether the playing piece has stopped (true without one)
bool MusicQueue::IsStopped()
{
    if (this->current.type == MusicSlot::Stream)
    {
        return this->streams.IsStopped(this->current.id);
    }

    if (this->current.type == MusicSlot::Midi)
    {
        return this->midi.IsStopped(this->current.id);
    }

    return true;
}

// swd3e.exe 0x0049D3B0 (the same code as BattleUnit::Data, the field at +0x0C): the list playing
int MusicQueue::Mode() const
{
    return this->mode;
}

}  // namespace swd3e
