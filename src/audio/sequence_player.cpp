#include "audio/sequence_player.h"

#include <algorithm>

#include "file/file.h"

namespace swd3e
{

namespace
{

constexpr int kStreamNodes = 2;

}  // namespace

// swd3e.exe 0x0049DD30
int ClampVolume(int volume)
{
    if (volume > SequenceNode::MaxLevel)
    {
        return SequenceNode::MaxLevel;
    }

    if (volume < 0)
    {
        return 0;
    }

    return volume;
}

// swd3e.exe 0x0049DD50: the pan -63..64 to the Miles range 0..127
int ClampPan(int pan)
{
    const int value = pan + SequenceNode::CenterPan;

    if (value > SequenceNode::MaxLevel)
    {
        return SequenceNode::MaxLevel;
    }

    if (value < 0)
    {
        return 0;
    }

    return value;
}

// swd3e.exe 0x0049E3D0: the volume removed per update to fade out in `steps` updates (0 steps count as 1: the whole
// volume at once), at least 1
int SetFadeStep(SequenceNode& node, int steps)
{
    const int step = node.volume / (steps != 0 ? steps : 1);
    node.fadeStep = step;

    if (step == 0)
    {
        node.fadeStep = 1;
    }

    return step;
}

// swd32.exe 0x00486580 (unmatched in swd3e)
SequencePlayer::SequencePlayer(AudioDevice& device)
    : device(device)
{
}

SequencePlayer::~SequencePlayer() = default;

// swd3e.exe 0x0049C9B0
bool SequencePlayer::IsOpen() const
{
    return this->opened;
}

// swd3e.exe 0x0049C9C0
bool SequencePlayer::IsEnabled() const
{
    return this->enabled;
}

const std::string& SequencePlayer::Error() const
{
    return this->error;
}

// swd3e.exe 0x0049CD60: the head of the free list
std::unique_ptr<SequenceNode> SequencePlayer::AllocNode()
{
    if (this->free.empty())
    {
        return nullptr;
    }

    std::unique_ptr<SequenceNode> node = std::move(this->free.back());
    this->free.pop_back();
    return node;
}

// swd3e.exe 0x0049E530: the node becomes the head of the active list
SequenceNode* SequencePlayer::PushActive(std::unique_ptr<SequenceNode> node)
{
    if (node == nullptr)
    {
        return nullptr;
    }

    this->active.push_front(std::move(node));
    return this->active.front().get();
}

// swd3e.exe 0x0049CD30: the node becomes the head of the free list
bool SequencePlayer::FreeNode(std::unique_ptr<SequenceNode> node)
{
    if (node == nullptr)
    {
        return false;
    }

    this->free.push_back(std::move(node));
    return true;
}

MidiPlayer::MidiPlayer(AudioDevice& device)
    : SequencePlayer(device)
{
}

// swd3e.exe 0x0049C920
MidiPlayer::~MidiPlayer()
{
    this->Close();
}

// swd32.exe 0x00484DD0 (unmatched in swd3e): the MIDI mapper, then device 0; one sequence handle
bool MidiPlayer::Open(int)
{
    if (!this->device.OpenMidi())
    {
        this->error = this->device.LastError();

        if (!this->device.OpenMidi())
        {
            this->error += " //";
            this->error += this->device.LastError();
            this->enabled = false;
            return false;
        }
    }

    this->opened = true;
    this->enabled = true;
    auto node = std::make_unique<SequenceNode>();
    node->voice = this->device.CreateVoice();
    this->free.push_back(std::move(node));
    return true;
}

// swd3e.exe 0x0049C930
int MidiPlayer::Close()
{
    if (!this->IsOpen())
    {
        return 0;
    }

    for (const auto& node : this->active)
    {
        this->device.End(node->voice);
        this->device.ReleaseVoice(node->voice);
    }

    this->active.clear();

    for (const auto& node : this->free)
    {
        this->device.ReleaseVoice(node->voice);
    }

    this->free.clear();
    this->opened = false;
    this->enabled = false;
    return 1;
}

// swd3e.exe 0x0049CA10: loads a MIDI file and starts it as sequence `id`. A file that cannot be opened loses the node;
// an initialization result of -1 is reported but played anyway. (Without a MIDI device the player never opens and has
// no free node.)
// swd3e.exe EH funclet (tail) of 0x0049CA10, swd32 0x004983F0 (EH unwind: frees the node allocated for the sequence,
// held by its unique_ptr)
// swd3e.exe EH funclet (tail) of 0x0049CA10, swd32 0x004983FB (EH handler of 0x00484F60)
int MidiPlayer::Play(const std::u8string& fileName, int id, int volume, int loops)
{
    if (id == 0)
    {
        return 0;
    }

    std::unique_ptr<SequenceNode> node = this->AllocNode();

    if (node == nullptr)
    {
        return 0;
    }

    node->midi.clear();
    node->fadeStep = 0;
    File file;

    if (!file.Open(fileName, File::Disposition::OpenAlways, File::Access::Read))
    {
        this->error = this->device.LastError();
        return 0;
    }

    u32 size = file.Size();
    node->midi.assign(size, 0);
    file.Read(node->midi.data(), size);
    file.Close();

    if (!this->device.LoadCompressed(node->voice, node->midi.data(), node->midi.size()))
    {
        this->error = this->device.LastError();
        node->midi.clear();
        this->FreeNode(std::move(node));
        return 0;
    }

    node->volume = SequenceNode::VolumeScale * ClampVolume(volume);
    node->id = id;
    this->device.SetVolume(node->voice, node->volume / SequenceNode::VolumeScale);
    this->device.SetLoops(node->voice, loops);
    this->device.Start(node->voice);
    this->PushActive(std::move(node));
    return id;
}

// swd3e.exe 0x0049C9D0: the XMIDI master volume (0..0x7F) and the volume the driver reports back; 0 while closed
int MidiPlayer::SetMasterVolume(int volume)
{
    if (!this->IsOpen())
    {
        return 0;
    }

    this->masterVolume = ClampVolume(volume);
    return this->masterVolume;
}

// swd3e.exe 0x0049CCA0: the fade step of `node` from the time asked, kept within the time left and 0x1E..0xF000 ms
int MidiPlayer::SetFadeStep(SequenceNode& node, int milliseconds)
{
    constexpr int kFadeRange = 0x800;
    constexpr int kLongestShift = 11;
    int total = 0;
    int current = 0;
    this->device.Position(node.voice, total, current);
    int span = std::min(milliseconds, total - current);
    const int unit = this->unknown4;
    span = std::max(span, unit);
    span = std::min(span, unit << kLongestShift);
    node.fadeStep = kFadeRange / (span / unit);
    return node.fadeStep;
}

// swd3e.exe 0x0049CC20: sequence `id` marked fading over `milliseconds`; swd3e has no MIDI update, so its volume never
// falls. 0 while closed or disabled, for id 0 or a sequence not playing.
int MidiPlayer::FadeOut(int id, int milliseconds)
{
    if (!this->IsOpen() || !this->IsEnabled() || id == 0)
    {
        return 0;
    }

    SequenceNode* node = this->Find(id);

    if (node == nullptr)
    {
        return 0;
    }

    node->flags |= SequencePlayer::Fading;
    this->SetFadeStep(*node, milliseconds);
    return id;
}

// swd3e.exe 0x0049CD00: without an id, whether nothing plays
bool MidiPlayer::IsStopped(int id)
{
    if (id != 0)
    {
        return this->Find(id) == nullptr;
    }

    return this->active.empty();
}

// swd3e.exe 0x0049CD80
SequenceNode* MidiPlayer::Find(int id)
{
    for (const auto& node : this->active)
    {
        if (node->id == id)
        {
            return node.get();
        }
    }

    return nullptr;
}

StreamPlayer::StreamPlayer(AudioDevice& device)
    : SequencePlayer(device)
{
}

// swd3e.exe 0x0049E0B0
StreamPlayer::~StreamPlayer()
{
    this->Close();
}

// swd3e.exe 0x0049E0C0: two stream slots on the digital driver of the sound effects
bool StreamPlayer::Open()
{
    this->opened = true;
    this->enabled = true;

    for (int i = 0; i < kStreamNodes; ++i)
    {
        this->free.push_back(std::make_unique<SequenceNode>());
    }

    return true;
}

// swd3e.exe 0x0049E120
int StreamPlayer::Close()
{
    if (!this->IsOpen())
    {
        return 0;
    }

    for (const auto& node : this->active)
    {
        if (node->voice != nullptr)
        {
            this->device.End(node->voice);
        }
    }

    this->active.clear();
    this->free.clear();
    this->opened = false;
    this->enabled = false;
    return 1;
}

// swd3e.exe 0x0049E1B0: the volume Miles reports back; -1 for an unknown id
int StreamPlayer::SetVolume(int id, int volume)
{
    if (!this->IsOpen())
    {
        return 0;
    }

    SequenceNode* node = this->Find(id);

    if (node == nullptr)
    {
        return -1;
    }

    node->volume = SequenceNode::VolumeScale * ClampVolume(volume);
    this->device.SetVolume(node->voice, ClampVolume(volume));
    return this->device.Volume(node->voice);
}

// swd3e.exe 0x0049E220: opens and starts the stream (no open or enabled check)
int StreamPlayer::Play(const std::u8string& fileName, int id, int volume, int loops)
{
    if (id == 0 || this->Find(id) != nullptr)
    {
        return 0;
    }

    std::unique_ptr<SequenceNode> node = this->AllocNode();

    if (node == nullptr)
    {
        return 0;
    }

    node->name.clear();
    node->fadeStep = 0;
    node->voice = this->device.OpenStream(fileName);

    if (node->voice == nullptr)
    {
        this->error = this->device.LastError();
        this->FreeNode(std::move(node));
        return 0;
    }

    node->name = fileName;
    node->volume = SequenceNode::VolumeScale * ClampVolume(volume);
    node->id = id;
    this->device.SetVolume(node->voice, ClampVolume(volume));
    this->device.SetLoops(node->voice, loops);
    this->device.Start(node->voice);
    this->PushActive(std::move(node));
    return id;
}

// swd3e.exe 0x0049E350: fades the stream out over `steps` updates
int StreamPlayer::FadeOut(int id, int steps)
{
    if (!this->IsOpen() || !this->IsEnabled() || id == 0)
    {
        return 0;
    }

    SequenceNode* node = this->Find(id);

    if (node == nullptr)
    {
        return 0;
    }

    int total = 0;
    int current = 0;
    this->device.Position(node->voice, total, current);
    node->flags |= SequencePlayer::Fading;
    SetFadeStep(*node, steps);
    return id;
}

// swd3e.exe 0x0049E400: fades and retires the streams (a finished or faded out stream is closed); a stream in an
// unexpected state is retired when the one before it was, a flag the original seeds with an uninitialised stack slot
// (false here, see deviations.md). The open and enabled flags are not checked.
bool StreamPlayer::Update()
{
    if (this->active.empty())
    {
        return false;
    }

    bool retiredLast = false;

    for (auto node = this->active.begin(); node != this->active.end();)
    {
        bool retire = false;
        SequenceNode& stream = **node;

        if ((stream.flags & SequencePlayer::Fading) == 0)
        {
            switch (this->device.Status(stream.voice))
            {
                case VoiceStatus::Done:
                    retire = true;
                    break;

                case VoiceStatus::Playing:
                case VoiceStatus::Stopped:
                case VoiceStatus::PlayingButReleased:
                    retiredLast = false;
                    break;

                default:
                    retire = retiredLast;
                    break;
            }
        }
        else
        {
            stream.volume -= stream.fadeStep;
            const int volume = ClampVolume(stream.volume / SequenceNode::VolumeScale);

            if (volume != 0)
            {
                this->device.SetVolume(stream.voice, volume);
                retiredLast = false;
            }
            else
            {
                retire = true;
            }
        }

        if (!retire)
        {
            ++node;
            continue;
        }

        retiredLast = true;
        this->device.End(stream.voice);
        stream.voice = nullptr;
        stream.name.clear();
        stream.fadeStep = 0;
        stream.flags = 0;
        this->free.push_back(std::move(*node));
        node = this->active.erase(node);
    }

    return false;
}

// swd3e.exe 0x0049E4F0
bool StreamPlayer::IsStopped(int id)
{
    return this->Find(id) == nullptr;
}

// swd3e.exe 0x0049E510: the id of the stream at the head of the active list (the latest started), 0 without one
int StreamPlayer::FirstId() const
{
    return this->active.empty() ? 0 : this->active.front()->id;
}

// swd3e.exe 0x0049E550
SequenceNode* StreamPlayer::Find(int id)
{
    for (const auto& node : this->active)
    {
        if (node->id == id)
        {
            return node.get();
        }
    }

    return nullptr;
}

}  // namespace swd3e
