#include "battle/battle_enemy.h"

#include <memory>

#include "org/database.h"
#include "res/act_archive.h"

namespace swd3e
{

namespace
{

constexpr u32 kProbedAction = 0x26;  // ProbeAct: an enemy's act without it sets unk3070

}  // namespace

// swd3e.exe 0x00487B90
BattleEnemy::BattleEnemy()
{
    this->ownedData = std::make_unique<ObjectRecord>();
    this->data = this->ownedData.get();
}

// swd3e.exe 0x00487BF0: the destructor: the enemy's mon.dat object freed (then the unit's, BattleUnit::~BattleUnit)
// swd3e.exe EH funclet (tail) of 0x00487BF0, swd32 0x004983B0 (EH unwind: the unit part, by BattleUnit::~BattleUnit)
// swd3e.exe EH funclet (tail) of 0x00487BF0, swd32 0x004983B8 (EH handler of 0x00475590)
BattleEnemy::~BattleEnemy()
{
    this->Release();
}

// swd3e.exe 0x00489410: the enemy's mon.dat object freed
void BattleEnemy::Release()
{
    this->ownedData.reset();
    this->data = nullptr;
}

// swd3e.exe 0x00487D60: an enemy entering battle: its placement, its mon.dat object (its name shown), its kind from the
// placement, its maximum hit points from its hit points, its level byte, its act (the object's, else the
// placement's) and skill script; then the act probed and the choices it has noted (inline in Reload too)
void BattleEnemy::Setup(const UnitPlacement& at, u16 id, Database& database, ActArchive& acts)
{
    this->placement = at;
    this->savedPlacement = this->placement;
    this->hitPointsOverride = 0;
    database.LoadObject(*this->data, id);
    this->SetDisplayName(this->data->data.name);
    ObjectData& object = this->data->data;
    this->kind = at.kind;
    object.maxHitPoints = object.hitPoints;
    this->evadeSkill = object.battleLevel;
    this->actId = object.act;

    if (this->actId == 0)
    {
        this->actId = at.actId;
    }

    this->unk2684 = 0;
    this->LoadCommand(database, object.skill);
    Database::FreeDescription(*this->data);
    this->ProbeAct(acts);
}

// The end of Setup and Reload (inline in each): unk3070 set when the act has no action 0x26 (the probe state
// cleared after), unk2FDD NoObjects when the object counts are all 0 and NoSkills when the skill counts are.
void BattleEnemy::ProbeAct(ActArchive& acts)
{
    this->unk3070 = 0;
    this->attackAct.id = this->actId;
    this->attackAct.action = kProbedAction;

    if (acts.Run(this->attackAct) == 0)
    {
        this->unk3070 = 1;
    }

    this->attackAct = ActState{};
    this->unk2FDD = 0;
    const ObjectData& object = this->data->data;

    if (object.itemACount == 0 && object.itemBCount == 0 && object.itemCCount == 0)
    {
        this->unk2FDD = BattleEnemy::NoObjects;
    }

    if (object.skillACount == 0 && object.skillBCount == 0)
    {
        this->unk2FDD = BattleEnemy::NoSkills;
    }
}

// swd3e.exe 0x00487E90: the enemy turned into object `id` (its hit points full, its level byte and skill script; its
// act the object's, else `id`), then the act probed and the choices noted as by Setup
void BattleEnemy::Reload(u16 id, Database& database, ActArchive& acts)
{
    database.LoadObject(*this->data, id);
    ObjectData& object = this->data->data;
    object.maxHitPoints = object.hitPoints;
    this->evadeSkill = object.battleLevel;
    this->LoadCommand(database, object.skill);
    Database::FreeDescription(*this->data);
    this->actId = object.act;

    if (this->actId == 0)
    {
        this->actId = id;
    }

    this->ProbeAct(acts);
}

}  // namespace swd3e
