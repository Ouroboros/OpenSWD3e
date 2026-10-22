#pragma once

#include <array>
#include <string_view>

#include "core/types.h"

namespace swd3e
{

// The talisman ghost (符鬼) the party carries (swd3e; 0x148 bytes at 0x004D0C10): one of five kinds, kept by kind in
// swdghost.dat (0x158 bytes a slot: the local time it was written, then the record). The meaning of most fields is
// not yet known; Stat reads them by number for the menus (0x0043F4A0) and the battle.
struct TalismanGhost
{
    // The kinds Reset makes (0x004C04B8..0x004C0498).
    static constexpr int Kinds = 5;
    static constexpr std::array<std::u8string_view, Kinds> KindNames =
        {u8"金符鬼", u8"木符鬼", u8"水符鬼", u8"火符鬼", u8"土符鬼"};
    static constexpr int ClearUnkB8 = 0x16;  // the Stat number that clears unkB8

    std::array<char, 0x10> name = {};  // +0x00
    i32 insight = 0;                   // +0x10 stat 3 悟性 (the ghost panel), 0x32 on Reset
    i32 unk14 = 0;                     // +0x14
    std::array<u8, 0x20> unk18 = {};   // +0x18
    std::array<u8, 0x20> unk38 = {};   // +0x38
    i32 unk58 = 0;                     // +0x58
    i32 unk5C = 0;                     // +0x5C
    i32 unk60 = 0;                     // +0x60
    i32 unk64 = 0;                     // +0x64
    i32 unk68 = 0;                     // +0x68
    i32 unk6C = 0;                     // +0x6C
    i32 unk70 = 0;                     // +0x70
    i32 unk74 = 0;                     // +0x74
    i32 unk78 = 0;                     // +0x78
    i32 unk7C = 0;                     // +0x7C
    i32 unk80 = 0;                     // +0x80
    i32 unk84 = 0;                     // +0x84
    i32 kind = 0;                      // +0x88 stat 0: 0..4 金, 木, 水, 火, 土 (Reset); its slot of swdghost.dat
    i32 unk8C = 0;                     // +0x8C stat 1
    i32 unk90 = 0;                     // +0x90 stat 2
    i32 unk94 = 0;                     // +0x94 stat 4
    i32 unk98 = 0;                     // +0x98 stat 5
    i32 unk9C = 0;                     // +0x9C stat 6
    i32 unkA0 = 0;                     // +0xA0
    i32 unkA4 = 0;                     // +0xA4
    i32 unkA8 = 0;                     // +0xA8
    i32 unkAC = 0;                     // +0xAC
    i32 unkB0 = 0;                     // +0xB0
    i32 unkB4 = 0;                     // +0xB4
    i32 unkB8 = 0;                     // +0xB8 cleared by Stat(0x16)
    i32 growthFrom = 0;                // +0xBC the days from which the satiety limit is 200 (inferred: 0x0044B670)
    i32 growthTo = 0;                  // +0xC0 the day after the last of them
    i32 cycleDays = 0;                 // +0xC4 the days of its cycle (0x18, 0x12 for 水); `day` steps round them
    i32 day = 0;                       // +0xC8 a step every 900 frames of the map (MainFrame)
    i32 unkCC = 0;                     // +0xCC
    i32 unkD0 = 0;                     // +0xD0
    i32 unkD4 = 0;                     // +0xD4
    i32 unkD8 = 0;                     // +0xD8
    i32 unkDC = 0;                     // +0xDC
    i32 unkE0 = 0;                     // +0xE0 stat 7
    i32 unkE4 = 0;                     // +0xE4 stat 8
    i32 unkE8 = 0;                     // +0xE8 stat 9 (1 for 金, 2 for the others)
    i32 unkEC = 0;                     // +0xEC stat 0xA
    i32 spirit = 0;                    // +0xF0 stat 0xB 靈力 (0xA for 金, 0x12 for the others); 5 x essence + 0..4 fed
    i32 strength = 0;                  // +0xF4 stat 0xC 力量 (0..500)
    i32 essence = 0;                   // +0xF8 stat 0xD 精魄 (0..100)
    i32 wisdom = 0;                    // +0xFC stat 0xE 智慧 (0..100)
    i32 agility = 0;                   // +0x100 stat 0xF 敏捷 (0..100)
    i32 unk104 = 0;                    // +0x104
    i32 unk108 = 0;                    // +0x108
    i32 unk10C = 0;                    // +0x10C
    i32 satiety = 0;                   // +0x110 fed points (0..200; falls by 3..7 every 900 frames)
    i32 unk114 = 0;                    // +0x114
    // +0x118 the skills it was fed (the ghost page shows three; Skill reads on into the fields after them)
    std::array<i32, 4> skills = {};
    i32 balance = 0;  // +0x128 平衡, the top of the ghost page's triangle (0..100)
    i32 attack = 0;   // +0x12C 攻擊 (0..100)
    i32 defense = 0;  // +0x130 防禦 (0..100)
    i32 unk134 = 0;   // +0x134
    i32 unk138 = 0;   // +0x138
    i32 unk13C = 0;   // +0x13C
    i32 unk140 = 0;   // +0x140
    i32 unk144 = 0;   // +0x144

    TalismanGhost();
    void Reset(u32 kind);
    i32 Stat(int number);
    i32 SetSkill(int index, i32 value);
    i32 Skill(int index) const;
    void Save(std::u8string_view directory, std::u8string_view file) const;
    void Load(std::u8string_view directory, std::u8string_view file, int slot);
};

static_assert(sizeof(TalismanGhost) == 0x148);

}  // namespace swd3e
