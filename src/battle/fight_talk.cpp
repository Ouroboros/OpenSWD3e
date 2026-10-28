#include "battle/fight_talk.h"

#include "core/format.h"

namespace swd3e
{

namespace
{

constexpr i32 kNoTalk = 0xFFFF;
constexpr u32 kSeekBytes = 0x1000;
constexpr int kByteBits = 8;

}  // namespace

// swd3e.exe 0x0047E270: drawMovedAtOnce cleared, then the talk script of battle `battle` (figtalk.dat is opened once
// and kept, created empty when missing; the battle's offset is the dword at 0x200 + 4 * battle)
void FightTalk::Load(const std::u8string& installDirectory, u16 battle)
{
    this->drawMovedAtOnce = 0;

    if (this->enabled == 0)
    {
        return;
    }

    if (!this->open)
    {
        // The original tests the handle against 0, so a failed open (-1) is kept as open and its reads fail: the
        // script is all zero.
        this->open = true;

        if (!this->file
                 .Open(Concat({installDirectory, u8"figtalk.dat"}), File::Disposition::OpenAlways, File::Access::Read))
        {
            this->script.assign(FightTalk::ScriptBytes, 0);
            this->cursor = 0;
            return;
        }

        this->file.Preload();
    }

    this->file.SeekBegin(static_cast<i32>(FightTalk::HeaderBytes + sizeof(u32) * static_cast<u32>(battle)));
    u32 offset = 0;
    this->file.ReadDword(offset);
    this->file.SeekBegin(static_cast<i32>(offset + FightTalk::HeaderBytes));
    this->script.assign(FightTalk::ScriptBytes, 0);
    this->cursor = 0;
    u32 bytes = FightTalk::ScriptBytes;
    this->file.Read(this->script.data(), bytes);
}

// swd3e.exe 0x0047E420: figtalk.dat closed and the talk state cleared (windowTimeout 0xFFFF, battleRuns and `enabled`
// 1), the script freed
void FightTalk::Close()
{
    if (this->file.IsOpen())
    {
        this->file.Close();
    }

    this->drawMovedAtOnce = 0;
    this->battleRuns = 1;
    this->enabled = 1;
    this->unk578414 = 0;
    this->unk578418 = 0;
    this->unk57841C = 0;
    this->unk578420 = 0;
    this->unk5782A4 = 0;
    this->unk5782A8 = 0;
    this->savedUnit = 0;
    this->battleActingUnit = 0;
    this->unk57842C = 0;
    this->unk57842E = 0;
    this->unk578430 = 0;
    this->windowFlags = 0;
    this->waitCount = 0;
    this->unk578448 = 0;
    this->unk57844C = 0;
    this->unk578450 = 0;
    this->unk578452 = 0;
    this->unk578454 = 0;
    this->unk578456 = 0;
    this->repeatLimit = 0;
    this->windowTimeout = kNoTalk;
    this->unk57845C = 0;
    this->unk57828C = 0;
    this->offset = 0;
    this->open = false;
    this->wonByTalk = 0;
    this->script.clear();
    this->cursor = 0;
}

// swd3e.exe 0x0047E3A0: kSeekBytes of the script read from `offset` (after the header) into a new buffer, read from
// its start
void FightTalk::Seek(i32 offset)
{
    this->script.assign(kSeekBytes, 0);
    this->cursor = 0;
    this->file.SeekBegin(offset + static_cast<i32>(FightTalk::HeaderBytes));
    u32 bytes = kSeekBytes;
    this->file.Read(this->script.data(), bytes);
    this->offset = offset;
}

// The signed word `index` of the command at the cursor (0 past the script; inline in 0x00469D20, which reads the
// buffer through a pointer).
i16 FightTalk::Word(std::size_t index) const
{
    return static_cast<i16>(this->UnsignedWord(index));
}

// The word `index` of the command at the cursor (0 past the script).
u16 FightTalk::UnsignedWord(std::size_t index) const
{
    const std::size_t at = this->cursor + index * sizeof(u16);

    if (at + sizeof(u16) > this->script.size())
    {
        return 0;
    }

    return static_cast<u16>(this->script[at] | (this->script[at + 1] << kByteBits));
}
}  // namespace swd3e
