#pragma once

#include "battle/battle_unit.h"

namespace swd3e
{

class ActArchive;

// An enemy in battle (0x3078 bytes in swd3e, twenty of them at 0x00539590): a unit with its own copy of the monster's
// object.
class BattleEnemy : public BattleUnit
{
public:
    // ObjectData::kind bit of the named characters and bosses (all with their own act).
    static constexpr u32 NamedKind = 0x20;
    // ObjectData::kind bit of the machines (蟹精魔機, 羯忍幻機, ...).
    static constexpr u32 MachineKind = 0x800;
    static constexpr u16 SmallMachineTypes = 0xA;
    // unk2FDD: the enemy has no object counts, or no skill counts.
    static constexpr u8 NoObjects = 1;
    static constexpr u8 NoSkills = 2;

    void Setup(const UnitPlacement& at, u16 id, Database& database, ActArchive& acts);
    void Reload(u16 id, Database& database, ActArchive& acts);
    BattleEnemy();
    ~BattleEnemy();
    void Release();

private:
    void ProbeAct(ActArchive& acts);
};

}  // namespace swd3e
