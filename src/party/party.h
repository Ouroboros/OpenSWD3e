#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "core/types.h"
#include "item/item_list.h"
#include "org/fame.h"
#include "text/big5.h"

namespace swd3e
{

class Database;
struct ObjectData;

// A party member (0x38 bytes in the original, four of them at 0x004AB790). The attribute names follow the status
// screen (attack and defence shown with their equipment bonus) and the battle checks of spirit and stamina costs;
// those marked inferred are not yet confirmed by a label.
struct RoleRecord
{
    static constexpr u16 Defeated = 0x8000;  // in `condition`: the role starts the battle defeated
    // swd3e: the resistance slot holding the role's element (0..4: 金 木 水 火 土; the menus name it, 0x0043F4A0)
    static constexpr std::size_t ElementAt = 10;

    i32 totalExperience = 0;  // +0x00 總經驗, raised by a won battle and checked against the next level
    i16 hitPoints = 0;        // +0x04 生命
    i16 spirit = 0;           // +0x06 靈力
    i16 stamina = 0;          // +0x08 體力
    i16 maxHitPoints = 0;     // +0x0A
    i16 maxSpirit = 0;        // +0x0C
    i16 maxStamina = 0;       // +0x0E
    i16 strength = 0;         // +0x10 力量
    i16 endurance = 0;        // +0x12 耐力
    i16 wisdom = 0;           // +0x14 智慧
    i16 agility = 0;          // +0x16 (inferred)
    i16 rapport = 0;          // +0x18 默契
    i16 reaction = 0;         // +0x1A 反應力
    i16 ailmentChance = 0;    // +0x1C an ailment lands on a roll of 1..10 up to it
    i16 evasion = 0;          // +0x1E 閃躲
    i32 experience = 0;       // +0x20 (inferred: the level.dat dword)
    u16 condition = 0;        // +0x24 Defeated
    u16 attackBonus = 0;      // +0x26 from the equipment, cleared in the save
    u16 defenseBonus = 0;     // +0x28 from the equipment, cleared in the save
    u16 unk2A = 0;            // +0x2A
    u8 level = 0;             // +0x2C
    // +0x2D by element, in tenths: swd3e keeps eleven (swd32 nine), the starting state copying the ninth into the
    // last two (0x0040E0D0)
    std::array<u8, 0xB> resistances = {};
    std::array<u8, 4> unk38 = {};  // +0x38

    void Load(std::span<const u8> record);
};

static_assert(sizeof(RoleRecord) == 0x3C);

// The 0x60-byte battle block of a party role (four of them at 0x004ACF50).
struct RoleBattleBlock
{
    std::array<i32, 0x18> values = {};  // by the battle's constants (the special points, the next special, ...)
};

// A role's name in three parts (swd3e; 0x20 bytes a role, the party's at 0x004E4C30). The parts are typed in boxes of
// 4, 4 and 8 bytes (talk command 209, the new game's name page); the menus show the surname and given name together.
struct RoleName
{
    // The parts as texts of the defaults (0x00429790).
    struct Parts
    {
        std::u8string_view surname;
        std::u8string_view given;
        std::u8string_view nickname;
    };

    std::array<char, 6> surname = {};      // +0x00
    std::array<char, 6> given = {};        // +0x06
    std::array<char, 0x14> nickname = {};  // +0x0C

    // A name of `parts` (strcpy of the Big5 texts into the cleared record).
    static RoleName From(const Parts& parts);
    // The text from a part up to its NUL; the original strcpy reads on into the next part when a part has none, here
    // the record ends it.
    std::string Surname() const;
    std::string Given() const;
    std::string Nickname() const;
    // The surname followed by the given name (strcpy and strcat of the menus).
    std::string FullName() const;

private:
    std::string TextAt(std::size_t offset) const;
};

static_assert(sizeof(RoleName) == 0x20);

// The data of the party saved with the game.
class Party
{
public:
    // swd32.exe 0x0049E344 (unmatched in swd3e): the object kind of each equipment slot (the kinds match in all of
    // their bits).
    static constexpr std::array<u32, 11> SlotKinds =
        {0x100, 0x10000200, 0x40000200, 0x20000200, 0x80000200, 0x400, 0x400, 0x40, 0x40, 0x800, 0x800};
    static constexpr int Roles = 4;
    // The names ResetNames gives the roles (0x004B2188..0x004B213C) and those the talk texts write for them (the
    // second set of 0x00429790; Talk::ExpandText puts the roles' names in their place).
    static constexpr std::array<RoleName::Parts, Roles> DefaultNames = {{
        {u8"陳", u8"靖仇", u8"阿仇"},
        {u8"于", u8"小雪", u8"阿雪"},
        {u8"拓跋", u8"玉兒", u8"阿玉"},
        {u8"張", u8"烈", u8"阿烈"},
    }};

    static constexpr std::array<RoleName::Parts, Roles> NameTags = {{
        {u8"▲▲", u8"靖仇", u8"阿仇"},
        {u8"陳美", u8"小雪", u8"阿雪"},
        {u8"拓跋", u8"玉兒", u8"阿玉"},
        {u8"張", u8"烈", u8"阿烈"},
    }};

    static constexpr int SlotsPerRole = 16;
    static constexpr int Variables = 64;
    // Party::SetRoleStat and RoleStat fields: the words of RoleRecord from hitPoints (the first SignedStats
    // signed), then the experience, the total experience and the level.
    static constexpr int SignedStats = 6;
    static constexpr int WordStats = 0xE;
    static constexpr int ExperienceStat = 0xE;
    static constexpr int TotalExperienceStat = 0xF;
    static constexpr int LevelStat = 0x10;
    static constexpr std::size_t Gold = 0;           // the variable that holds the gold
    static constexpr std::size_t GhostVariable = 2;  // swd3e: the talisman ghost chosen (1..5, 0: none)
    static constexpr int FirstRoleFlag = 0x1E;       // story flag FirstRoleFlag + n: party role n has joined
    // The variables talk command VariableNamePlate may name a plate by.
    static constexpr u16 FirstNameVariable = 0xB;
    static constexpr u16 SecondNameVariable = 0xC;

    std::array<RoleRecord, Roles> roles;                                    // swd32 0x004AB790
    ItemLink inventory;                                                     // 0x004C3018
    std::array<std::unique_ptr<ItemNode>, Roles> skills;                    // 0x004C2B68 each headed by "無"
    std::array<std::unique_ptr<ItemNode>, Roles * SlotsPerRole> equipment;  // 0x004E4CB0
    static constexpr u32 ClockLimit = 1000;                                 // the clock goes back to 0 past it
    // swd32 0x004ACDB0 the game clock: a tick every Game::ClockFrames + 1 frames on the map (talk commands
    i32 clock = 0;
    // 0x22..0x25)
    std::array<i32, Variables> variables = {};  // 0x004C6370 [0]: the gold
    Fame fame;
    std::array<RoleName, Roles> names = {};     // 0x004E4C30
    std::array<RoleName, Roles> nameTags = {};  // 0x004E4AF8 the names as the talk texts write them

    void ResetNames();
    static constexpr int LastStat = 0x10;  // the attributes SetRoleStat and RoleStat know
    void SetRoleStat(int field, int value, int role, Database& database);
    int RoleStat(int field, int role) const;
    std::array<RoleBattleBlock, Roles> battleBlocks;  // 0x004C6700
};

void ApplyObject(RoleRecord& record, const ObjectData& object, Database& database);

}  // namespace swd3e
