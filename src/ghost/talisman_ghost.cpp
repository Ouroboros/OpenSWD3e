#include "ghost/talisman_ghost.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "file/file.h"
#include "platform/system_info.h"
#include "text/big5.h"

namespace swd3e
{

namespace
{

constexpr u32 kSlotBytes = 0x158;  // swdghost.dat: the time written, then the record
constexpr u32 kTimeBytes = 0x10;   // a SYSTEMTIME
constexpr u32 kLastKind = TalismanGhost::Kinds - 1;

// The values Reset gives every kind
constexpr i32 kStartUnk10 = 0x32;
constexpr i32 kStartUnk60 = 0x5A;
constexpr i32 kStartUnk64 = 0x64;
constexpr i32 kStartUnk68 = 1;
constexpr i32 kStartUnk6C = 0x5A;
constexpr i32 kStartUnk70 = 0x64;
constexpr i32 kStartUnk78 = 0xE;
constexpr i32 kStartUnk8C = 1;
constexpr i32 kStartStat = 0x32;  // unk90, unk94, unk98 and unk9C
constexpr i32 kStartUnkBC = 6;
constexpr i32 kStartUnkC0 = 0xA;
constexpr i32 kStartUnkCC = 0x258;
constexpr i32 kStartUnkE4 = 1;
constexpr i32 kStartUnkEC = 1;
constexpr i32 kStartUnkF4 = 0x14;
constexpr i32 kStartUnkF8 = 0xF;
constexpr i32 kStartUnkFC = 8;
constexpr i32 kStartUnk100 = 6;
constexpr i32 kStartUnk114 = 0x28;

// The values Reset gives by kind
struct KindStart
{
    i32 unkC4;
    i32 unkE8;
    i32 unkF0;
};

constexpr std::array<KindStart, TalismanGhost::Kinds> kKindStarts = {{
    {0x18, 1, 0xA},
    {0x18, 2, 0x12},
    {0x12, 2, 0x12},
    {0x18, 2, 0x12},
    {0x18, 2, 0x12},
}};

// The numbers Stat reads
enum class GhostStat
{
    Kind = 0,
    Unk8C = 1,
    Unk90 = 2,
    Unk10 = 3,
    Unk94 = 4,
    Unk98 = 5,
    Unk9C = 6,
    UnkE0 = 7,
    UnkE4 = 8,
    UnkE8 = 9,
    UnkEC = 0xA,
    UnkF0 = 0xB,
    UnkF4 = 0xC,
    UnkF8 = 0xD,
    UnkFC = 0xE,
    Unk100 = 0xF,
};

}  // namespace

// swd3e.exe 0x0049E7F0: a ghost of the last kind (土符鬼) made over the cleared record
TalismanGhost::TalismanGhost()
{
    this->Reset(kLastKind);
}

// swd3e.exe 0x0049EBB0: the record made a new ghost of `kind` (0..4, others leave it): its name and starting values.
// The fields Reset does not name keep what they held.
void TalismanGhost::Reset(u32 kind)
{
    if (kind > kLastKind)
    {
        return;
    }

    this->unk18 = {};
    this->unk38 = {};
    this->unk60 = kStartUnk60;
    this->unk64 = kStartUnk64;
    this->unk68 = kStartUnk68;
    this->unk6C = kStartUnk6C;
    this->unk70 = kStartUnk70;
    this->unk78 = kStartUnk78;
    this->unk7C = 0;
    this->kind = static_cast<i32>(kind);
    this->unk8C = kStartUnk8C;
    this->unk90 = kStartStat;
    this->unk10 = kStartUnk10;
    this->unk94 = kStartStat;
    this->unk98 = kStartStat;
    this->unk9C = kStartStat;
    this->unkA0 = 0;
    this->unkA4 = 0;
    this->unkAC = 0;
    this->unkB0 = 0;
    this->unkCC = kStartUnkCC;
    this->unkD0 = 0;
    const KindStart& start = kKindStarts[kind];
    this->unkE4 = kStartUnkE4;
    this->unkE8 = start.unkE8;
    this->unkEC = kStartUnkEC;
    this->unkE0 = 0;
    this->unkF0 = start.unkF0;
    this->unkF4 = kStartUnkF4;
    this->unkF8 = kStartUnkF8;
    this->unkFC = kStartUnkFC;
    this->unk100 = kStartUnk100;
    this->unk110 = 0;
    this->unk114 = kStartUnk114;
    this->unkBC = kStartUnkBC;
    this->unkC0 = kStartUnkC0;
    this->unkC4 = start.unkC4;
    this->unkC8 = 0;
    this->unk118 = {};
    const std::string name = Big5(TalismanGhost::KindNames[kind]);
    std::copy(name.begin(), name.end(), this->name.begin());
    this->name[name.size()] = '\0';
    this->unk128 = 0;
    this->unk12C = 0;
    this->unk130 = 0;
}

// swd3e.exe 0x0049E800: a field by number (0 for numbers it does not know; number 0x16 clears unkB8)
i32 TalismanGhost::Stat(int number)
{
    switch (static_cast<GhostStat>(number))
    {
        case GhostStat::Kind:
            return this->kind;

        case GhostStat::Unk8C:
            return this->unk8C;

        case GhostStat::Unk90:
            return this->unk90;

        case GhostStat::Unk10:
            return this->unk10;

        case GhostStat::Unk94:
            return this->unk94;

        case GhostStat::Unk98:
            return this->unk98;

        case GhostStat::Unk9C:
            return this->unk9C;

        case GhostStat::UnkE0:
            return this->unkE0;

        case GhostStat::UnkE4:
            return this->unkE4;

        case GhostStat::UnkE8:
            return this->unkE8;

        case GhostStat::UnkEC:
            return this->unkEC;

        case GhostStat::UnkF0:
            return this->unkF0;

        case GhostStat::UnkF4:
            return this->unkF4;

        case GhostStat::UnkF8:
            return this->unkF8;

        case GhostStat::UnkFC:
            return this->unkFC;

        case GhostStat::Unk100:
            return this->unk100;

        default:
            break;
    }

    if (number == TalismanGhost::ClearUnkB8)
    {
        this->unkB8 = 0;
    }

    return 0;
}

// swd3e.exe 0x0049EFC0: value `index` of unk118 set (the original writes past the four as well; here those are
// dropped)
i32 TalismanGhost::SetUnk118(int index, i32 value)
{
    if (index >= 0 && static_cast<std::size_t>(index) < this->unk118.size())
    {
        this->unk118[static_cast<std::size_t>(index)] = value;
    }

    return value;
}

// swd3e.exe 0x0049EFE0: value `index` of unk118 (0 past the four, which the original reads beyond them)
i32 TalismanGhost::Unk118(int index) const
{
    if (index >= 0 && static_cast<std::size_t>(index) < this->unk118.size())
    {
        return this->unk118[static_cast<std::size_t>(index)];
    }

    return 0;
}

// swd3e.exe 0x0049E910: the record written to its kind's slot of `file` in `directory` (the installation; the file is
// made when missing), after the local time
void TalismanGhost::Save(std::u8string_view directory, std::u8string_view file) const
{
    const std::u8string path = std::u8string(directory) + std::u8string(file);
    File out;

    if (!out.Open(path, File::Disposition::OpenAlways, File::Access::Write))
    {
        return;
    }

    out.SeekBegin(static_cast<i32>(kSlotBytes) * this->kind);
    const LocalDateTime now = LocalTime();
    const std::array<u16, kTimeBytes / sizeof(u16)> time = {
        static_cast<u16>(now.year),
        static_cast<u16>(now.month),
        static_cast<u16>(now.dayOfWeek),
        static_cast<u16>(now.day),
        static_cast<u16>(now.hour),
        static_cast<u16>(now.minute),
        static_cast<u16>(now.second),
        static_cast<u16>(now.milliseconds),
    };

    u32 bytes = kTimeBytes;
    out.Write(time.data(), bytes);
    bytes = sizeof(TalismanGhost);
    out.Write(this, bytes);
    out.Close();
}

// swd3e.exe 0x0049EA80: the record read from slot `slot` of `file` in `directory` (the installation; left as it is
// when the file cannot be opened)
void TalismanGhost::Load(std::u8string_view directory, std::u8string_view file, int slot)
{
    const std::u8string path = std::u8string(directory) + std::u8string(file);
    File in;

    if (!in.Open(path, File::Disposition::OpenExisting, File::Access::Read))
    {
        return;
    }

    u32 bytes = sizeof(TalismanGhost);
    in.SeekBegin(static_cast<i32>(kSlotBytes) * slot + static_cast<i32>(kTimeBytes));
    in.Read(this, bytes);
    in.Close();
}

}  // namespace swd3e
