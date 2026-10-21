#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "game_data.h"
#include "ghost/talisman_ghost.h"
#include "test.h"
#include "text/big5.h"

using swd3e::TalismanGhost;
using swd3e::u32;
using swd3e::u8;

namespace
{

// The kind names Reset copies (0x0049EBB0).
constexpr std::array<u32, TalismanGhost::Kinds> kKindNamesAt =
    {0x004C04B8, 0x004C04B0, 0x004C04A8, 0x004C04A0, 0x004C0498};
constexpr std::size_t kSlotBytes = 0x158;
constexpr std::size_t kTimeBytes = 0x10;
constexpr swd3e::i32 kStartUnk10 = 0x32;
constexpr swd3e::i32 kWaterUnkC4 = 0x12;  // 水符鬼's
constexpr swd3e::i32 kMetalUnkF0 = 0xA;   // 金符鬼's

}  // namespace

TEST_CASE("ghost: the kind names match swd3e.exe and Reset makes each kind")
{
    const std::vector<u8> exe = swd3etest::ReadGameFile("swd3e.exe");

    if (exe.empty())
    {
        std::printf("  (swd3e.exe not found, skipped)\n");
        return;
    }

    for (std::size_t kind = 0; kind < kKindNamesAt.size(); ++kind)
    {
        CHECK(swd3etest::SameText(exe, kKindNamesAt[kind], TalismanGhost::KindNames[kind]));
    }

    TalismanGhost ghost;
    CHECK_EQ(ghost.kind, TalismanGhost::Kinds - 1);
    ghost.Reset(2);
    CHECK_EQ(ghost.kind, 2);
    CHECK_EQ(ghost.unkC4, kWaterUnkC4);
    CHECK_EQ(ghost.Stat(3), kStartUnk10);
    CHECK(std::string(ghost.name.data()) == swd3e::Big5(TalismanGhost::KindNames[2]));
    ghost.Reset(TalismanGhost::Kinds);
    CHECK_EQ(ghost.kind, 2);
}

TEST_CASE("ghost: a slot of swdghost.dat is the time, then the record")
{
    const std::vector<u8> file = swd3etest::ReadGameFile("swdghost.dat");

    if (file.size() < kSlotBytes)
    {
        std::printf("  (swdghost.dat not found, skipped)\n");
        return;
    }

    CHECK_EQ(file.size(), TalismanGhost::Kinds * kSlotBytes);
    TalismanGhost ghost;
    std::memcpy(&ghost, file.data() + kTimeBytes, sizeof(TalismanGhost));
    CHECK(std::string(ghost.name.data()) == swd3e::Big5(TalismanGhost::KindNames[0]));
    CHECK_EQ(ghost.kind, 0);
    CHECK_EQ(ghost.unkF0, kMetalUnkF0);
}
