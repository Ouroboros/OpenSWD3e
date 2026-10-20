#include "res/act_archive.h"

#include <cstring>

#include "core/byte_reader.h"
#include "core/format.h"
#include "core/words.h"
#include "memmgr/heap_block.h"
#include "platform/message_box.h"

namespace swd3e
{

namespace
{

constexpr int kDirectoryRecordBytes = 0x2C;
constexpr i32 kDirectoryBias = 0x10;        // the record of entry e at 0x2C * e - 0x10 (0x1C + 0x2C * (e - 1))
constexpr std::size_t kRecordSizeWord = 5;  // a record: its size and offset dwords
constexpr std::size_t kRecordOffsetWord = 6;
constexpr std::size_t kCountBytes = sizeof(u16);
constexpr i16 kDelayMask = 0x7FFF;  // a negative speed sets the delay to its low bits

int ChainOf(int id, int sub)
{
    return static_cast<int>(static_cast<u32>(id == ActArchive::MapAct ? sub : id) & ActArchive::ChainMask);
}

u16 ReadWord(const std::vector<u8>& data, std::size_t offset)
{
    return ByteReader::Word16At(data, offset);
}

u32 ReadDword(const std::vector<u8>& data, std::size_t offset)
{
    return ByteReader::Word32At(data, offset);
}

}  // namespace

ActArchive::ActArchive(ErrorReporter& errors)
    : errors(errors)
{
}

void ActArchive::SetGameDirectory(std::u8string directory)
{
    this->gameDirectory = std::move(directory);
}

void ActArchive::SetEffectStart(EffectStart start)
{
    this->effectStart = std::move(start);
}

int ActArchive::CacheBytes() const
{
    return static_cast<int>(this->cacheBytes);
}

// Set by a restart of Run and cleared once its script loaded (the original flag at 0x004F5160).
bool ActArchive::ScriptRestarted() const
{
    return this->restarted;
}

void ActArchive::SetScriptRestarted(bool value)
{
    this->restarted = value;
}

ActState ActArchive::Peek(const ActState& state)
{
    ActState next = state;
    const bool kept = this->restarted;
    this->peeking = true;
    this->Run(next);
    this->peeking = false;
    this->restarted = kept;
    return next;
}

// Opens the six archives from the game directory (inline at the start of 0x004377B0 and 0x00437920).
void ActArchive::OpenArchives()
{
    for (int i = 0; i < ActArchive::FileCount; ++i)
    {
        this->OpenFile(
            this->files[static_cast<std::size_t>(i)],
            PathFormat("%s%s", {this->gameDirectory, ActArchive::FileNames[static_cast<std::size_t>(i)]})
        );
    }

    this->opened = true;
}

// The archive of an id: ids below 0 and from 18000 on index outside the six handles in the original; they find no
// archive here (see deviations.md).
File* ActArchive::ArchiveOf(int id)
{
    const int index = id / ActArchive::IndicesPerFile;
    return id >= 0 && index < ActArchive::FileCount ? &this->files[static_cast<std::size_t>(index)] : nullptr;
}

// swd3e.exe 0x00436B40
void ActArchive::SetCacheLimit(int bytes)
{
    this->cacheLimit = bytes;
}

// swd3e.exe 0x00436B50: read-only, not shared
bool ActArchive::OpenFile(File& file, const std::u8string& name)
{
    if (!file.Open(name, File::Disposition::OpenExisting, File::Access::Read))
    {
        return false;
    }

    file.Preload();
    return true;
}

// swd3e.exe 0x00436B80: script `sub` of an entry, from its offset on: nothing for a sub beyond the count or a zero
// offset. The original then scans the following offsets for a non-zero one without using it.
ActScript ActArchive::Extract(const std::shared_ptr<const std::vector<u8>>& entry, int sub) const
{
    if (entry == nullptr)
    {
        return {};
    }

    const int count = ReadWord(*entry, 0);

    // A negative sub reads before the entry in the original (see deviations.md).
    if (sub >= count || sub < 0)
    {
        return {};
    }

    const u32 offset = ReadDword(*entry, kCountBytes + sizeof(u32) * static_cast<std::size_t>(sub));

    if (offset == 0)
    {
        return {};
    }

    return ActScript{.entry = entry, .offset = offset};
}

// swd3e.exe 0x00436BD0: reads the whole entry at its directory record (cached) into a new block
std::shared_ptr<const std::vector<u8>> ActArchive::LoadEntry(File* file, int fileIndex, int entry, u32& size)
{
    u32 length = 0;
    u32 offset = 0;
    this->IndexLookup(fileIndex, entry, offset, length);

    if (offset == 0)
    {
        this->IndexInsert(file, fileIndex, entry, offset, length);
    }

    size = length;
    // The block keeps its rounding: a script read past the entry reads its zeroed slack as the original does.
    std::shared_ptr<std::vector<u8>> data = std::make_shared<std::vector<u8>>();

    if (!HeapBlock::Allocate(*data, length))
    {
        data.reset();
    }

    if (file != nullptr)
    {
        file->Seek(File::Origin::Begin, static_cast<i32>(offset));

        if (data != nullptr)
        {
            u32 bytes = length;
            file->Read(data->data(), bytes);
        }
    }

    this->reads += 2;
    return data;
}

// swd3e.exe 0x004377B0: the cached script; on a miss the cache is trimmed once it holds the limit and the entry
// loaded
ActScript ActArchive::Get(int id, int sub)
{
    if (!this->opened)
    {
        this->OpenArchives();
    }

    if (this->CacheFind(id, sub))
    {
        return this->current;
    }

    if (static_cast<i32>(this->cacheBytes) >= this->cacheLimit)
    {
        this->CacheEvict();
    }

    return this->CacheLoad(id, sub) ? this->current : ActScript{};
}

// swd3e.exe 0x00437920: a script outside the cache (the original allocates a new entry on every call and keeps it)
ActScript ActArchive::LoadDirect(int id, int sub)
{
    if (!this->opened)
    {
        this->OpenArchives();
    }

    u32 size = 0;
    const auto entry =
        this->LoadEntry(this->ArchiveOf(id), id / ActArchive::IndicesPerFile, id % ActArchive::IndicesPerFile, size);
    this->direct = this->Extract(entry, sub);
    return this->direct;
}

// swd3e.exe 0x00437AA0: the entry is loaded, then put at the front of its chain
bool ActArchive::CacheLoad(int id, int sub)
{
    u32 size = 0;
    const auto entry =
        this->LoadEntry(this->ArchiveOf(id), id / ActArchive::IndicesPerFile, id % ActArchive::IndicesPerFile, size);
    this->current = ActScript{.entry = entry, .offset = 0};

    if (entry == nullptr)
    {
        return false;
    }

    const int chain = ChainOf(id, sub);
    ++this->chainCounts[static_cast<std::size_t>(chain)];
    this->chains[static_cast<std::size_t>(chain)].push_front(CacheEntry{.id = id, .size = size, .data = entry});
    this->cacheBytes += size;
    this->current = this->Extract(entry, sub);
    return this->current.entry != nullptr;
}

// swd3e.exe 0x00437BB0: an entry of the id (whatever sub it was loaded for) moves to the front of its chain and gives
// the script
bool ActArchive::CacheFind(int id, int sub)
{
    this->current = {};
    std::list<CacheEntry>& entries = this->chains[static_cast<std::size_t>(ChainOf(id, sub))];

    for (auto entry = entries.begin(); entry != entries.end(); ++entry)
    {
        if (entry->id == id)
        {
            entries.splice(entries.begin(), entries, entry);
            this->current = this->Extract(entries.front().data, sub);
            return true;
        }
    }

    return false;
}

// swd3e.exe 0x00437C50: drops entries from the end of the longest chain (the first of equal ones) until below the limit
// or the chain is empty
void ActArchive::CacheEvict()
{
    std::size_t longest = 0;

    for (std::size_t i = 0; i < this->chainCounts.size(); ++i)
    {
        if (this->chainCounts[i] > this->chainCounts[longest])
        {
            longest = i;
        }
    }

    std::list<CacheEntry>& entries = this->chains[longest];

    while (static_cast<i32>(this->cacheBytes) >= this->cacheLimit)
    {
        if (!entries.empty())
        {
            this->cacheBytes -= entries.back().size;
            entries.pop_back();
        }

        if (--this->chainCounts[longest] == 0)
        {
            break;
        }
    }
}

// swd3e.exe 0x00437D10
void ActArchive::CacheClear()
{
    for (std::size_t i = 0; i < this->chains.size(); ++i)
    {
        for (const CacheEntry& entry : this->chains[i])
        {
            this->cacheBytes -= entry.size;
            --this->chainCounts[i];
        }

        this->chains[i].clear();
    }
}

// swd3e.exe 0x00437D80: closes everything; a cache that still counts bytes afterwards is reported
int ActArchive::CloseAll()
{
    for (File& file : this->files)
    {
        file.Close();
    }

    this->CacheClear();
    this->IndexClear();
    this->opened = false;
    this->reads = 0;
    this->effectValue = 0;
    this->effectX = 0;
    this->effectY = 0;

    if (this->cacheBytes != 0)
    {
        this->errors.ShowError(ActArchive::MemoryErrorTitle, ActArchive::MemoryError);
        return 1;
    }

    return 0;
}

// swd3e.exe 0x00437E00: 1 makes Run take scripts from the cache, anything else loads them directly
void ActArchive::SetUseCache(int value)
{
    this->useCache = value;
}

// swd3e.exe 0x00437E10: 1 makes 'CS' start its effect for every act
void ActArchive::SetEffectEnabled(int value)
{
    this->effectEnabled = value;
}

// swd3e.exe 0x00437E20: reads the directory record of the entry (at the full entry number) into the front of its
// index chain
void ActArchive::IndexInsert(File* file, int fileIndex, int entry, u32& offset, u32& size)
{
    const auto chain = static_cast<std::size_t>(static_cast<u32>(entry) & ActArchive::ChainMask);
    ++this->indexCounts[chain];
    u32 record[kDirectoryRecordBytes / sizeof(u32)] = {};

    if (file != nullptr)
    {
        file->Seek(File::Origin::Begin, kDirectoryRecordBytes * entry - kDirectoryBias);
        u32 bytes = kDirectoryRecordBytes;
        file->Read(record, bytes);
    }

    this->reads += 2;
    this->index[chain].push_front(IndexEntry{fileIndex, entry, record[kRecordOffsetWord], record[kRecordSizeWord]});
    offset = record[kRecordOffsetWord];
    size = record[kRecordSizeWord];
}

// swd3e.exe 0x00437F30: a found record moves to the front of its chain
void ActArchive::IndexLookup(int fileIndex, int entry, u32& offset, u32& size)
{
    std::list<IndexEntry>& records =
        this->index[static_cast<std::size_t>(static_cast<u32>(entry) & ActArchive::ChainMask)];

    for (auto record = records.begin(); record != records.end(); ++record)
    {
        if (record->file == fileIndex && record->entry == entry)
        {
            records.splice(records.begin(), records, record);
            offset = records.front().offset;
            size = records.front().size;
            return;
        }
    }
}

// swd3e.exe 0x00437FC0
void ActArchive::IndexClear()
{
    for (std::size_t i = 0; i < this->index.size(); ++i)
    {
        this->indexCounts[i] = static_cast<u16>(this->indexCounts[i] - this->index[i].size());
        this->index[i].clear();
    }
}

// The playback reset of a new direction, action or id (inline in 0x00436C80).
void ActArchive::Restart(ActState& state, u32 keptFlags)
{
    state.delay = 0;
    state.frameDelay = 0;
    state.position = 0;
    state.effectA = 0;
    state.effectQ = 0;
    state.x = 0;
    state.y = 0;
    state.width = 0;
    state.height = 0;
    state.view = 0;
    state.target = 0;
    state.anchorX = 0;
    state.anchorY = 0;
    state.signal = 0;
    state.wait = 0;
    state.wavePhase = 0;
    state.drawLevel = 0;
    state.red = 0;
    state.green = 0;
    state.blue = 0;

    for (u16& light : state.lights)
    {
        light = 0;
    }

    state.loopCount = 0;
    state.loopStart = 0;
    state.loopEnd = 0;
    state.waiting = 0;
    state.hold = 0;
    state.mask = 0;
    state.flags &= keptFlags;
    this->restarted = true;
}

// swd3e.exe 0x00436C80: one game frame of an action script. A new direction, action or id restarts it. The steps up
// to 'DE' run at once and set the delay of the frame. 0 when the script cannot be loaded. A repeat set by 'PL' ends
// the frame at 'DE', 'VO' and '2O' by the round of the counter: at the start round the frame ends there, at the end
// round the script starts again (counting the repeat down unless it repeats forever), past the start round the frame
// ends, before it the view and target are cleared and the steps go on. The opcodes YX, HW, GC, RC and BC read their
// second value without moving past it, so that value also runs as the next opcode. Words beyond the entry (read from
// the heap in the original) end the frame here.
int ActArchive::Run(ActState& state)
{
    if ((state.hold == 1 && state.frame != 0) || state.id == 0)
    {
        return 1;
    }

    if (state.direction != state.lastDirection)
    {
        state.lastDirection = state.direction;
        this->Restart(state, ActState::KeptOnTurn);
    }

    if (state.action != state.lastAction)
    {
        state.lastAction = state.action;
        this->Restart(state, ActState::KeptOnRestart);
    }

    if (state.id != state.lastId)
    {
        state.lastId = state.id;
        this->Restart(state, ActState::KeptOnRestart);
    }

    const int sub = state.action + state.direction;
    state.script = this->useCache == 1 ? this->Get(state.id, sub) : this->LoadDirect(state.id, sub);

    if (state.script == nullptr)
    {
        return 0;
    }

    this->restarted = false;

    if (state.delay != 0)
    {
        --state.delay;
        return 1;
    }

    const std::vector<u8>& entry = *state.script.entry;
    const std::size_t base = state.script.offset;

    auto word = [&entry, base](u16 index) -> u16
    {
        const std::size_t at = base + sizeof(u16) * static_cast<std::size_t>(index);

        if (at + sizeof(u16) > entry.size())
        {
            return static_cast<u16>(ActOpcode::EndFrame);
        }

        u16 value = 0;
        std::memcpy(&value, entry.data() + at, sizeof(u16));
        return value;
    };

    // The repeat at a 'DE', 'VO' or '2O': true when the frame ends (the position set), false to clear the view and
    // target and go on.
    enum class Repeat
    {
        Ends,
        GoesOn,
    };

    auto repeat = [&state](u16 stay) -> Repeat
    {
        const auto round = static_cast<u8>(state.counter >> ActState::RoundShift);

        if (round == state.loopStart)
        {
            return Repeat::Ends;
        }

        if (round == state.loopEnd)
        {
            if ((state.loopCount & ActState::LoopForever) == 0)
            {
                --state.loopCount;

                if ((state.loopCount & ActState::LoopMask) == 0)
                {
                    state.loopCount = 0;
                    state.position = stay;
                    return Repeat::Ends;
                }
            }

            state.position = 0;
            return Repeat::Ends;
        }

        if (round > state.loopStart)
        {
            return Repeat::Ends;
        }

        state.view = 0;
        state.target = 0;
        return Repeat::GoesOn;
    };

    state.actionCode = 0;

    while (true)
    {
        const auto code = static_cast<ActOpcode>(word(state.position));
        const auto next = static_cast<u16>(state.position + 1);
        state.position = next;

        if (code == ActOpcode::EndFrame)
        {
            if (state.loopCount == 0)
            {
                if (state.hold == 1)
                {
                    state.position = static_cast<u16>(next - 1);
                }

                return 1;
            }

            // At the end round 'DE' leaves the position after it when the count runs out.
            if (repeat(next) == Repeat::Ends)
            {
                return 1;
            }

            continue;
        }

        if (code == ActOpcode::Loop)
        {
            if (state.loopCount == 0)
            {
                state.position = state.hold == 1 ? static_cast<u16>(next - 1) : 0;
                return 1;
            }

            if (repeat(0) == Repeat::Ends)
            {
                return 1;
            }

            continue;
        }

        if (code == ActOpcode::Stop)
        {
            if (state.loopCount == 0)
            {
                if (state.delay == 0)
                {
                    state.waiting = 1;
                }

                state.position = static_cast<u16>(next - 1);
                return 1;
            }

            if (repeat(static_cast<u16>(next - 1)) == Repeat::Ends)
            {
                return 1;
            }

            continue;
        }

        state.delay = state.frameDelay;
        const i16 speed = state.speed;

        if (speed < 0)
        {
            state.delay = static_cast<u16>(speed & kDelayMask);
        }

        switch (code)
        {
            case ActOpcode::View:
                state.view = word(state.position++);
                break;

            case ActOpcode::Position:
                state.x = word(state.position++);
                state.y = word(state.position);
                break;

            case ActOpcode::Size:
                state.width = word(state.position++);
                state.height = word(state.position);
                break;

            case ActOpcode::Counter:
                state.counter = word(state.position++);
                break;

            case ActOpcode::Wait:
                state.wait = static_cast<u8>(word(state.position++));
                break;

            case ActOpcode::Invisible:
                state.flags |= ActState::Invisible;
                break;

            case ActOpcode::Mask:
                state.mask = 1;
                break;

            case ActOpcode::Frame:
                state.frame = word(state.position++);
                break;

            case ActOpcode::Rotation:
                state.rotation = word(state.position++);
                break;

            case ActOpcode::Effect:
            {
                const u16 value = word(state.position);
                state.unknown5E = value;
                state.unknown60 = value;

                if ((state.unknown94 == 1 || this->effectEnabled == 1) && !this->peeking)
                {
                    state.flags |= ActArchive::FlagEffect;

                    if (this->effectStart)
                    {
                        this->effectStart(
                            state.x,
                            state.y,
                            static_cast<i16>(state.unknown5E),
                            static_cast<i16>(state.unknown60)
                        );
                    }
                }

                if (!this->peeking)
                {
                    this->effectValue = state.unknown5E;
                    this->effectX = LowWord(static_cast<u32>(state.x));
                    this->effectY = LowWord(static_cast<u32>(state.y));
                }

                ++state.position;
                break;
            }

            case ActOpcode::Delay:
                state.frameDelay = word(state.position++);
                state.delay = state.frameDelay;

                if (speed < 0)
                {
                    state.delay = static_cast<u16>(speed & kDelayMask);
                }

                break;

            case ActOpcode::EffectQ:
                state.effectQ = word(state.position++);
                break;

            case ActOpcode::OffsetX:
                state.offsetX = word(state.position++);
                break;

            case ActOpcode::OffsetY:
                state.offsetY = word(state.position++);
                break;

            case ActOpcode::Picture:
                state.picture = word(state.position++);
                state.counter = static_cast<u16>(
                    (state.counter & ActState::RoundsMask) |
                    ((state.counter & ~ActState::RoundsMask) + ActState::RoundStep)
                );
                break;

            case ActOpcode::Action:
                state.actionCode = word(state.position++);
                break;

            case ActOpcode::Signal:
                state.flags |= ActState::Signal;
                state.signal = static_cast<u8>(word(state.position++));
                break;

            case ActOpcode::DrawLevel:
                state.flags |= ActState::DrawLevel;
                state.drawLevel = static_cast<u8>(word(state.position++));
                break;

            case ActOpcode::Visible:
                state.flags &= ~ActState::Invisible;
                break;

            case ActOpcode::LoopRange:
            {
                state.loopStart = static_cast<u8>(word(state.position++));
                state.loopEnd = static_cast<u8>(word(state.position++));

                if ((state.loopCount & ActState::LoopActive) == 0)
                {
                    state.loopCount = static_cast<u16>(word(state.position) | ActState::LoopActive);
                }

                ++state.position;
                break;
            }

            case ActOpcode::Lights:
            {
                const u16 at = state.position;

                for (int i = 0; i < ActState::Lights; ++i)
                {
                    state.lights[i] = word(static_cast<u16>(at + i));
                }

                state.position = static_cast<u16>(at + ActState::Lights);
                break;
            }

            case ActOpcode::Green:
                state.green = word(state.position++);
                state.green2 = word(state.position);
                break;

            case ActOpcode::ClearColors:
                state.red2 = 0;
                state.green2 = 0;
                state.blue2 = 0;
                break;

            case ActOpcode::Red:
                state.red = word(state.position++);
                state.red2 = word(state.position);
                break;

            case ActOpcode::Blue:
                state.blue = word(state.position++);
                state.blue2 = word(state.position);
                break;

            case ActOpcode::EffectA:
                state.effectA = word(state.position++);
                break;

            case ActOpcode::Half:
                state.flags |= ActState::Half;
                break;

            case ActOpcode::MaskBlend:
                state.flags |= ActState::MaskBlend;
                break;

            case ActOpcode::NoBlend:
                state.flags |= ActState::NoBlend;
                break;

            case ActOpcode::Target:
                state.target = word(state.position++);
                break;

            case ActOpcode::AnchorX:
                state.anchorX = word(state.position++);
                break;

            case ActOpcode::AnchorY:
                state.anchorY = word(state.position++);
                break;

            default:
                break;
        }
    }
}

}  // namespace swd3e
