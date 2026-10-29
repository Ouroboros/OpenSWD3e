#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "core/types.h"

namespace swd3e
{

#pragma pack(push, 1)

// The fixed part of a mon.dat object (an item, a skill or a monster; 0xA4 of the 0xA8 bytes of the original swd3e
// record, swd32's 0xA0), filled tag by tag by Database::LoadObject. Offsets are given for reference; names follow the
// bestiary and battle screens, the rest are placeholders.
struct ObjectData
{
    static constexpr u16 FirstRoleBit = 0x8000;  // in `usableBy`: the bit of the first role, the others below it
    static constexpr u32 UnkKind20 = 0x20;       // in `kind`: scripted acts and absorbs do not affect it
    static constexpr u32 Absorbable = 0x800;     // in `kind`: an ordinary monster (蟹精魔機, 黏怪, 吸血殭屍, ...)
    static constexpr u32 UnkKind8000 = 0x8000;   // in `kind`: cleared on a skill learnt by a talk command

    std::array<char, 0x20> name = {};  // +0x00 tag 0x3E8 (a longer name runs into the next fields)
    u32 kind = 0;                      // +0x20 tag 0x19
    u16 attackBonusUp = 0;             // +0x24 tag 7 added to the attack bonus (a guest role: its growth in tenths)
    u16 defenseBonusUp = 0;            // +0x26 tag 8 added to the defence bonus
    u16 strengthUp = 0;                // +0x28 tag 0xF added to the strength (worn: to the attack bonus too)
    u16 enduranceUp = 0;               // +0x2A tag 0x10 added to the endurance (worn: to the defence bonus too)
    u16 agilityUp = 0;                 // +0x2C tag 9 added to the agility (a guest role: its growth in tenths)
    u16 wisdomUp = 0;                  // +0x2E tag 0x11 added to the wisdom
    u16 rapportUp = 0;                 // +0x30 tag 0x12 added to the rapport
    u16 evasionUp = 0;                 // +0x32 tag 0xA added to the evasion
    u16 hitPointsUp = 0;               // +0x34 tag 0x13 added to the hit points (or their maximum by useKind)
    u16 spiritUp = 0;                  // +0x36 tag 0x14 the spirit
    u16 staminaUp = 0;                 // +0x38 tag 0x15 the stamina
    u16 usableBy = 0;                  // +0x3A tag 0x1A the roles that can learn or use it (FirstRoleBit >> role)
    u16 useKind = 0;                   // +0x3C tag 0x1B how it is used: what it raises (vitals or maxima), its uses
    u16 skillScript = 0;               // +0x3E tag 0x64 the skill script it runs (mon.dat)
    u16 costBits = 0;                  // +0x40 tag 6 a skill's cost (bit 15 spirit, bit 14 stamina), an item's use bits
    u16 masteryValue = 0;              // +0x42 tag 0xC what its counter (counterMax) leads to: a weapon's damage
                                       // bonus once fully grown, the object it turns into, a special skill's next
                                       // special (0: the last)
    u16 counterMax = 0;                // +0x44 tag 0xD the top of its counter (proficiency)
    u16 price = 0;                     // +0x46 tag 0xB (the shop value)
    u16 actionKind = 0;                // +0x48 tag 0x16 the action it takes: a weapon's strong attack act, a
                                       // magic's cast action, an item's script (0: skillScript), an accessory's
                                       // kind (AccessoryKind)
    u16 unk4A = 0;                     // +0x4A tag 2 (swd3e)
    u16 unk4C = 0;                     // +0x4C tag 3 (swd3e)
    u16 unk4E = 0;                     // +0x4E
    i32 maxHitPoints = 0;              // +0x50 an enemy's maximum (set in battle, 0x00487DBD)
    // +0x54..+0xA2: the 0x4F-byte block of tag 1; tags 0xE, 0x7D0, 0x18, 0x1C and 0x1D set parts of it. Against
    // swd32 the fields from act on are 4 bytes later, those from itemA on 6 (hitPoints grew to a dword).
    u16 act = 0;            // +0x54 tag 0xE its act (an accessory copy keeps its item id there)
    u16 category = 0;       // +0x56 tag 0x7D0 the altar category, an enemy's type
    u16 level = 0;          // +0x58 tag 0x7D0 (shown by the bestiary)
    u16 attack = 0;         // +0x5A
    u16 defense = 0;        // +0x5C
    u16 agility = 0;        // +0x5E the battle speed
    u16 unk60 = 0;          // +0x60
    u16 ailmentChance = 0;  // +0x62 an ailment lands on a roll of 1..10 up to it
    u16 skill = 0;          // +0x64 an enemy's skill script (mon.dat)
    u16 unk66 = 0;          // +0x66
    i32 hitPoints = 0;      // +0x68 (read as a dword by the bestiary, 0x00442920)
    u16 itemA = 0;          // +0x6C the objects an enemy uses (inferred) and their counts; set together by
                            // BattleUnit::ChooseCommand
    u16 itemACount = 0;
    u16 itemB = 0;
    u16 itemBCount = 0;
    u16 itemC = 0;
    u16 itemCCount = 0;
    u16 skillA = 0;  // +0x78 the skills of an enemy and their counts, set together
    u16 skillACount = 0;
    u16 skillB = 0;
    u16 skillBCount = 0;
    u16 healItemA = 0;   // +0x80 the object an enemy heals itself with (choice 1)
    u16 healCountA = 0;  // +0x82 its uses left
    u16 healItemB = 0;
    u16 healCountB = 0;       // +0x86 (choice 2)
    u16 dropItem = 0;         // +0x88 the object an enemy may drop (RollDrop)
    u16 dropChance = 0;       // +0x8A a roll of 0..19 below it drops it
    u16 stealItem = 0;        // +0x8C the item an enemy can be robbed of (BattleRole::Steal), 0: none
    u16 stealDifficulty = 0;  // +0x8E 2..5: a steal roll below it succeeds
    u16 unk90 = 0;            // +0x90 (swd3e) the gold an enemy stole (EnemyHitDamage)
    u16 unk92 = 0;
    // +0x94..+0x97 tag 0x18 (and tag 1's block): an enemy's enrage (0x0049BB50, 0x0049BE20)
    u16 enrageCondition = 0;  // +0x94 1..0x1E: what enrages the enemy (EnrageCheck)
    u16 enrageEffect = 0;     // +0x96 below 1001: a skill it takes up; 1001..: a stat or flag it gains (Enrage)
    u8 itemChance = 0;        // +0x98 0: the enemy uses no objects, else added to the turns without one for the roll
    u8 skillChance = 0;       // +0x99 the same for the skills
    u8 battleLevel = 0;       // +0x9A a unit's evasion skill in battle (also read by 0x00414DB0)
    u8 catchChance = 0;       // +0x9B near enough, a catch roll of 0..9 below it catches the enemy
    // +0x9C by element (金 木 水 火 土); tag 0x7D0 sets them
    std::array<u8, 5> resistances = {};
    u8 altar = 0;  // +0xA1 tags 0x1C, 0x7D0 the altar it belongs to
    u8 unkA2 = 0;  // +0xA2 tags 0x1D, 0x7D0
    u8 unkA3 = 0;  // +0xA3
};

#pragma pack(pop)

static_assert(sizeof(ObjectData) == 0xA4);
static_assert(offsetof(ObjectData, maxHitPoints) == 0x50);
static_assert(offsetof(ObjectData, act) == 0x54);
static_assert(offsetof(ObjectData, hitPoints) == 0x68);
static_assert(offsetof(ObjectData, itemA) == 0x6C);
static_assert(offsetof(ObjectData, enrageCondition) == 0x94);
static_assert(offsetof(ObjectData, itemChance) == 0x98);
static_assert(offsetof(ObjectData, catchChance) == 0x9B);
static_assert(offsetof(ObjectData, resistances) == 0x9C);
static_assert(offsetof(ObjectData, altar) == 0xA1);

// A mon.dat object: the fixed part and the description (+0xA4, tag 0x1E; none when the record has no such tag).
struct ObjectRecord
{
    ObjectData data;
    std::optional<std::string> description;
};

// The name of `record` up to its terminator, within its 0x20 bytes (the original copies with lstrcpyA, on into the
// next fields when no terminator is there).
inline std::string ObjectName(const ObjectRecord& record)
{
    const auto& name = record.data.name;
    std::size_t length = 0;

    while (length < name.size() && name[length] != '\0')
    {
        ++length;
    }

    return std::string(name.data(), length);
}

// A skill script of mon.dat (Database::LoadSkill): flags and parameters set by the opcodes.
struct SkillScript
{
    // The values of `ailment` (the status bit it sets on the unit); names are given where the battle shows them.
    enum Ailment : u16
    {
        UnkAilment1 = 1,
        UnkAilment2 = 2,
        UnkAilment4 = 4,
        UnkAilment8 = 8,
        UnkAilment10 = 0x10,
        UnkAilment20 = 0x20,
        SlowedAilment = 0x40,
        UnkAilment80 = 0x80,
        UnkAilment100 = 0x100,
        StatsScaledAilment = 0x200,
        UnkAilment400 = 0x400,
        PoisonAilment = 0x1000,         // swd3e: a poison of the skill's turns (AddAilment)
        LastingPoisonAilment = 0x2000,  // swd3e: a poison that does not wear off (AddAilment)
        RefreshOnly = 0x8000,           // an effect of the same kind only gets new turns and value
    };

    // The bits of `flags`, each set by an opcode of the skill script.
    enum Flag : u32
    {
        TargetsAll = 1,    // opcode 3: every unit of the side
        AllAtOnce = 2,     // opcode 9: all hit at once
        UnkFlag4 = 4,      // opcode 0xB
        ElementBuff = 8,   // opcode 0xE: the buff raises the resistance to `element`
        UnkFlag10 = 0x10,  // opcodes 0xF..0x15 set 0x10..0x800 in turn (0x80 excepted)
        UnkFlag20 = 0x20,
        UnkFlag40 = 0x40,
        NoAilmentFlag = 0x80,  // opcode 6: the skill brings no ailment
        UnkFlag100 = 0x100,
        UnkFlag200 = 0x200,
        UnkFlag400 = 0x400,
        UnkFlag800 = 0x800,
        UnkFlag1000 = 0x1000,      // opcode 0x16, with unk22 and turns
        UnkFlag2000 = 0x2000,      // opcode 0x18
        UnkFlag4000 = 0x4000,      // opcode 0x19
        UnkFlag8000 = 0x8000,      // opcode 0x1B
        UnkFlag10000 = 0x10000,    // opcode 0x1C
        UnkFlag20000 = 0x20000,    // opcode 0xC when statusBits has the bit
        UnkFlag40000 = 0x40000,    // opcode 0x1D
        UnkFlag80000 = 0x80000,    // opcode 0x1E
        UnkFlag100000 = 0x100000,  // opcode 0x1F, with unk24
    };

    static constexpr u16 Unk20Mark = 0x8000;        // set in unk20 by opcode 0xA
    static constexpr u32 StatusUnk20000 = 0x20000;  // in statusBits: opcode 0xC also sets UnkFlag20000

    // The values of `element` (names from the battle messages of each resistance).
    enum Element : u16
    {
        Metal = 1,  // swd3e: the five elements 金 木 水 火 土
        Wood = 2,
        Water = 3,
        Fire = 4,
        Earth = 5,
    };

    // The bits of `statusBits` (kept by the effect nodes of a unit, EffectNode::bits).
    enum Buff : u32
    {
        EarthAttackUp = 0x4000,        // swd3e
        DispelBuffs = 0x10000,         // swd3e: every buff and lowering is taken off
        DamageTransferBuff = 0x20000,  // swd3e
        StrongAttackBuff = 0x40000,    // swd3e
        DefenseDown = 0x80000,
        SpeedDown = 0x100000,
        FireAttackUp = 0x200000,  // swd3e: the attack buffs of the elements
        WaterAttackUp = 0x400000,
        WoodAttackUp = 0x800000,
        MetalAttackUp = 0x1000000,
        Barrier = 0x2000000,
        ResistAllBuff = 0x4000000,  // swd3e: 抵抗所有攻擊
        AttackDown = 0x8000000,
        Invisible = 0x10000000,
        DefenseUp = 0x20000000,
        SpeedUp = 0x40000000,
        AttackUp = 0x80000000,
    };

    u32 unk00 = 0;
    u32 flags = 0;       // +0x04 opcodes 3, 6, 9, 0xB, 0xC, 0xE..0x16, 0x18, 0x19, 0x1B..0x1F
    u32 statusBits = 0;  // +0x08 opcode 0xC: the status bits of a buff
    u32 unk0C = 0;       // +0x0C opcode 0
    u32 unk10 = 0;       // +0x10 opcode 0
    u16 unk14 = 0;       // +0x14 opcode 8
    u16 unk16 = 0;       // +0x16 opcode 4
    u16 unk18 = 0;       // +0x18 opcode 0x1A (new in swd3e)
    u16 ailment = 0;     // +0x1A opcodes 6, 7 (bit 13: none, bit 15: refresh only)
    u16 element = 0;     // +0x1C opcode 0xE: the element of a resistance buff
    u16 unk1E = 0;       // +0x1E opcode 0xD
    u16 unk20 = 0;       // +0x20 opcode 0xA (bit 15 set with it)
    u16 unk22 = 0;       // +0x22 opcode 0x16
    u16 unk24 = 0;       // +0x24 opcode 0x1F (new in swd3e)
    u16 unk26 = 0;       // +0x26 opcode 0x17, first value (new in swd3e)
    u16 unk28 = 0;       // +0x28 opcode 0x17, second value
    u8 turns = 0;        // +0x2A opcodes 2, 0x16: the turns of an effect
};

static_assert(sizeof(SkillScript) == 0x2C);

// The opcodes of a skill script of mon.dat (Database::LoadSkill); 5 ends the script.
enum class SkillOpcode : u16
{
    Values = 0,  // unk0C and unk10
    Turns = 2,
    SetUnkFlag1 = 3,
    Unk16 = 4,
    End = 5,
    NoAilment = 6,
    Ailment = 7,
    Unk14 = 8,
    SetUnkFlag2 = 9,
    Unk20 = 0xA,
    SetUnkFlag4 = 0xB,
    StatusBits = 0xC,
    Unk1E = 0xD,
    Element = 0xE,
    SetUnkFlag10 = 0xF,
    SetUnkFlag20 = 0x10,
    SetUnkFlag40 = 0x11,
    SetUnkFlag100 = 0x12,
    SetUnkFlag200 = 0x13,
    SetUnkFlag400 = 0x14,
    SetUnkFlag800 = 0x15,
    Unk22 = 0x16,
    Unk26 = 0x17,
    SetUnkFlag2000 = 0x18,
    SetUnkFlag4000 = 0x19,
    Unk18 = 0x1A,
    SetUnkFlag8000 = 0x1B,
    SetUnkFlag10000 = 0x1C,
    SetUnkFlag40000 = 0x1D,
    SetUnkFlag80000 = 0x1E,
    Unk24 = 0x1F,
};

// The tags of an object script of mon.dat (Database::LoadObject); 5 ends the script.
enum class ObjectTag : u16
{
    ActBlock = 1,  // 0x4F bytes from act (swd32: 0x4D)
    Unk4A = 2,     // swd3e
    Unk4C = 3,     // swd3e
    End = 5,
    CostBits = 6,
    AttackBonusUp = 7,
    DefenseBonusUp = 8,
    AgilityUp = 9,
    EvasionUp = 0xA,
    Price = 0xB,
    MasteryValue = 0xC,
    CounterMax = 0xD,
    Act = 0xE,
    StrengthUp = 0xF,
    EnduranceUp = 0x10,
    WisdomUp = 0x11,
    RapportUp = 0x12,
    HitPointsUp = 0x13,
    SpiritUp = 0x14,
    StaminaUp = 0x15,
    ActionKind = 0x16,
    Enrage = 0x18,  // swd3e: the enrage condition and effect (two words from enrageCondition)
    Kind = 0x19,
    UsableBy = 0x1A,
    UseKind = 0x1B,
    Altar = 0x1C,
    UnkA2 = 0x1D,
    Description = 0x1E,
    SkillScript = 0x64,
    Name = 0x3E8,
    Resistances = 0x7D0,  // swd3e: the last five resistances, the altar, unkA2, the level and category
};

}  // namespace swd3e
