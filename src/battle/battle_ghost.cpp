#include <algorithm>
#include <array>
#include <string>

#include "audio/audio_manager.h"
#include "battle/battle_role.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/battle_unit_constants.h"
#include "battle/smooth/smooth_draw.h"
#include "config/config.h"
#include "ghost/talisman_ghost.h"
#include "org/database.h"
#include "party/party.h"
#include "random/random.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

// swd3e: the talisman ghost fighting with the party (a BattleRole set up by SetupGhost) and the orders the player gives
// it, with the helpers the battle frames ask of the roles acting by themselves.
namespace swd3e
{

namespace
{

constexpr u16 kSkillOrder = 1;  // the orders of the ghost (ghostOrder; meanings by TakeGhostOrder)
constexpr u16 kRestOrder = 2;
constexpr u16 kAttackOrder = 3;
constexpr u16 kGuardOrder = 4;  // keeps the ghost by its master (GhostGuards)
constexpr u16 kFifthOrder = 5;
constexpr u16 kSixthOrder = 6;
constexpr u16 kSeventhOrder = 7;
constexpr u16 kEighthOrder = 8;
constexpr u32 kHalfRoll = 100;  // RollHalf: a roll of 0..99 of 50 or more
constexpr i16 kHalfRollFrom = 50;
constexpr i32 kTrustedSkill = 0x33;   // the ghost's unk104 from which a skill of TakeSkillUsed counts
constexpr i32 kTrustedAttack = 0x51;  // from which a skill of TakeAttackUsed and TakeHealUsed counts
constexpr u32 kObeyRoll = 100;
constexpr u32 kRestoreRoll = 10;  // RestoreGhostSpirit: 5..14 percent of the maximum spirit
constexpr i16 kRestoreBase = 5;
constexpr int kPercent = 100;
constexpr u32 kFedSkills = 3;  // ChooseGhostSkill picks one of the first three fed skills
constexpr u16 kCostMask = 0x7FFF;
constexpr u32 kHealKind = 4;  // ObjectData::kind bits of the ghost's skills
constexpr u32 kAttackKind = 8;
constexpr u32 kAttackKinds = 0x18;
constexpr u32 kSkillKind = 0x1000;
constexpr u16 kChosenSkill = 0x8000;  // ghostSkill: the skill index was chosen
constexpr u16 kSkillIndex = 0x7FFF;
constexpr int kFirstFed = 0;  // the fed skills ChooseGhostSkillOf looks at (two of them; mode 1 the third)
constexpr int kThirdFed = 2;
constexpr int kLookedFed = 2;
constexpr int kThirdMode = 1;           // ChooseGhostSkillOf: the third fed skill
constexpr int kHealMode = 2;            // one of the first two of kind 4
constexpr int kAttackMode = 3;          // one of the first two of kind 8 or 0x10
constexpr u16 kHealPercentUse = 0x200;  // ObjectData::useKind: the heals are percentages
constexpr std::size_t kClearedName = 0x10;
constexpr u16 kGhostHeal = 0x237E;  // the spirit gain number
constexpr int kAttackCommand = 1;
constexpr int kSkillCommand = 2;
constexpr u16 kWalkingFlags = 0x18;  // UnitCommand::flags (UnkFlag8 | 0x10): the ghost walks to the target to attack
constexpr u16 kWalkCommand = 0xC8;   // the walk command (as BattleRole::PrepareCommand)
constexpr int kSpendShare = 40;      // SpendGhostSpirit: below 40 percent of the cost the ghost does nothing
constexpr int kElementAccessory = 0x26;      // SetElement: the accessory kind whose proficiency scales the resistance
constexpr std::size_t kBattleElementAt = 9;  // SetElement: the battle record keeps its element in resistance 9
// (+0x36), the party record in resistance 10 (+0x37, RoleRecord::ElementAt); the resistance taken is unk38[0] (+0x38)
constexpr u32 kGapRoll = 100;
constexpr int kWideGap = 0x14;  // LevelGapRoll: the level gaps and the chances of each
constexpr u16 kWideGapChance = 0x55;
constexpr int kEmptyGap = 0xF;
constexpr u16 kEmptyGapChance = 0x32;
constexpr int kWeakGap = 0xA;
constexpr u16 kWeakGapChance = 0x28;
constexpr int kWeakShare = 3;
constexpr u16 kNearGap = 8;
constexpr u16 kMiddleGapFrom = 9;
constexpr u16 kMiddleGapTo = 0xF;
constexpr u16 kMiddleGapChance = 0xA;
constexpr u16 kFarGapFrom = 0x10;
constexpr u16 kFarGapTo = 0x13;
constexpr u16 kFarGapChance = 0x14;
constexpr i16 kHalfShare = 2;
constexpr i16 kThirdShare = 3;
constexpr i16 kQuarterShare = 4;
constexpr i32 kSpecialAct = 0x17BF;  // StartSpecialAct: slot act 10 of equipment flag 0x80
constexpr std::size_t kSpecialSlot = 10;
constexpr int kSpecialPan = 0x30;             // SpecialActFrame: its sound panned right
constexpr int kSpecialBaseShare = 40;         // the hit points it stands up with, in percent of the maximum
constexpr int kSpecialProficiencyShare = 90;  // and the proficiency percent of this percent of it

}  // namespace

// swd3e.exe 0x004864C0: the order the player gave the ghost (1..8); order 4 keeps it by its master
void BattleRole::SetGhostOrder(u16 order)
{
    this->unk2BDA = static_cast<u16>(this->unk2BDA & ~BattleUnit::GhostGuards);
    this->ghostOrder = order;

    if (order == kGuardOrder)
    {
        this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::GhostGuards);
    }
}

// swd3e.exe 0x004864F0
u16 BattleRole::GhostOrder() const
{
    return this->ghostOrder;
}

// swd3e.exe 0x00486500: a roll of 0..99 of 50 or more
int BattleRole::RollHalf(BattleServices& services)
{
    return static_cast<i16>(services.random.Below(kHalfRoll)) >= kHalfRollFrom ? 1 : 0;
}

// swd3e.exe 0x00486520
bool BattleRole::GhostOrderIs5() const
{
    return this->ghostOrder == kFifthOrder;
}

// swd3e.exe 0x00486530
bool BattleRole::GhostOrderIs6() const
{
    return this->ghostOrder == kSixthOrder;
}

// swd3e.exe 0x00486540: the skill chosen this turn (GhostSkillUsed, taken back): true when the ghost's unk104 is at
// least 0x33
bool BattleRole::TakeSkillUsed()
{
    if ((this->unk2BDA & BattleUnit::GhostSkillUsed) == 0)
    {
        return false;
    }

    this->unk2BDA = static_cast<u16>(this->unk2BDA & ~BattleUnit::GhostSkillUsed);
    return this->ghostRecord->unk104 >= kTrustedSkill;
}

// swd3e.exe 0x00486570: the attack skill chosen this turn (GhostAttackUsed, taken back): true under orders 1 and 3
// when the ghost's unk104 is at least 0x51
bool BattleRole::TakeAttackUsed()
{
    if ((this->unk2BDA & BattleUnit::GhostAttackUsed) == 0)
    {
        return false;
    }

    this->unk2BDA = static_cast<u16>(this->unk2BDA & ~BattleUnit::GhostAttackUsed);
    const u16 order = this->ghostOrder;
    return (order == kSkillOrder || order == kAttackOrder) && this->ghostRecord->unk104 >= kTrustedAttack;
}

// swd3e.exe 0x004865B0: the heal skill chosen this turn (GhostHealUsed, taken back): true when the ghost's unk104 is
// at least 0x51
bool BattleRole::TakeHealUsed()
{
    if ((this->unk2BDA & BattleUnit::GhostHealUsed) == 0)
    {
        return false;
    }

    this->unk2BDA = static_cast<u16>(this->unk2BDA & ~BattleUnit::GhostHealUsed);
    return this->ghostRecord->unk104 >= kTrustedAttack;
}

// swd3e.exe 0x004865E0: order 7 given this turn (GhostOrder7, taken back)
int BattleRole::TakeOrder7()
{
    if ((this->unk2BDA & BattleUnit::GhostOrder7) == 0)
    {
        return 0;
    }

    this->unk2BDA = static_cast<u16>(this->unk2BDA & ~BattleUnit::GhostOrder7);
    return 1;
}

// swd3e.exe 0x00486600: a ghost keeping by its master (GhostGuards) obeys on a roll of 1..100 up to its unk104;
// otherwise it takes the weak stand pose (unk2BDA bit 0)
int BattleRole::GhostObeys(BattleServices& services)
{
    if ((this->unk2BDA & BattleUnit::GhostGuards) == 0)
    {
        return 0;
    }

    if (static_cast<i16>(services.random.Below(kObeyRoll) + 1) > this->ghostRecord->unk104)
    {
        this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::WeakStand);
        return 0;
    }

    return 1;
}

// swd3e.exe 0x00486640: under order 2 (GhostRests) the ghost gains 5..14 percent of its maximum spirit, shown as a
// spirit gain
int BattleRole::RestoreGhostSpirit(BattleServices& services)
{
    if ((this->unk2BDA & BattleUnit::GhostRests) == 0)
    {
        return 0;
    }

    const int gain =
        static_cast<i16>(services.random.Below(kRestoreRoll) + kRestoreBase) * this->battleRecord->maxSpirit / kPercent;
    this->PushTarget(kGhostHeal);
    this->SetDamageDigits(services, static_cast<i16>(gain));
    this->StepTargetCount(services, 1);
    this->Damage(services, 0, static_cast<i16>(gain), 0);
    return 1;
}

// swd3e.exe 0x004866C0: `target` already has an effect of the command's ailment or of its unk08
int BattleRole::HasCommandEffect(const BattleUnit& target) const
{
    if (target.HasEffectBits(this->command.ailment) == 1)
    {
        return 1;
    }

    return target.HasEffectBits(static_cast<u32>(this->command.unk08)) == 1 ? 1 : 0;
}

// swd3e.exe 0x00486700
int BattleRole::SetGhostActs(int value)
{
    this->ghostActs = value;
    return value;
}

// swd3e.exe 0x00486710: when the ghost acts this turn, one of its first three fed skills taken at random (none: it
// does not act); with the spirit for it, its script becomes the command and its kind marks it (GhostAttackUsed for kind
// 8, GhostHealUsed for kind 4, GhostSkillUsed for kind 0x1000)
int BattleRole::ChooseGhostSkill(BattleServices& services, Database& database)
{
    if (this->ghostActs != 1)
    {
        return 1;
    }

    this->ghostSkill = static_cast<u16>(services.random.Below(kFedSkills));
    const i32 skill = this->ghostRecord->skills[this->ghostSkill];

    if (skill == 0)
    {
        this->ghostActs = 0;
        return 1;
    }

    database.LoadObject(this->unk2B24, static_cast<u16>(skill));
    Database::FreeDescription(this->unk2B24);
    this->unk2F10 = static_cast<u16>(this->unk2B24.data.costBits & kCostMask);

    if (this->battleRecord->spirit < this->unk2F10)
    {
        return 1;
    }

    this->command = UnitCommand{};
    this->LoadCommand(database, this->unk2B24.data.skillScript);
    const u32 kind = this->unk2B24.data.kind;

    if ((kind & kAttackKind) != 0)
    {
        this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::GhostAttackUsed);
    }

    if ((kind & kHealKind) != 0)
    {
        this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::GhostHealUsed);
    }

    if ((kind & kSkillKind) != 0)
    {
        this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::GhostSkillUsed);
    }

    return 1;
}

// The fed skill of `index` loaded into unk2B24 with its spirit cost in unk2F10 (inline in ChooseGhostSkillOf and
// GhostSkillCommand); true when the ghost has the spirit for it.
bool BattleRole::LoadGhostSkill(Database& database, i32 skill)
{
    database.LoadObject(this->unk2B24, static_cast<u16>(skill));
    Database::FreeDescription(this->unk2B24);
    this->unk2F10 = static_cast<u16>(this->unk2B24.data.costBits & kCostMask);
    return this->battleRecord->spirit >= this->unk2F10;
}

// swd3e.exe 0x004867F0: a fed skill chosen by `mode`: 1 the third (GhostSkillUsed); 2 one of the first two of kind 4
// (GhostHealUsed); 3 one of the first two of kind 8 or 0x10 (GhostAttackUsed), the two at random when both qualify.
// With the spirit for it its script becomes the command (1); else (or none) the command and the cost are cleared (0).
int BattleRole::ChooseGhostSkillOf(BattleServices& services, Database& database, int mode)
{
    auto none = [this]
    {
        this->unk2F10 = 0;
        this->command = UnitCommand{};
        return 0;
    };

    auto take = [this, &database](u16 flag)
    {
        this->command = UnitCommand{};
        this->LoadCommand(database, this->unk2B24.data.skillScript);
        this->unk2BDA = static_cast<u16>(this->unk2BDA | flag);
    };

    if (mode == kThirdMode)
    {
        const i32 skill = this->ghostRecord->skills[kThirdFed];

        if (skill == 0 || !this->LoadGhostSkill(database, skill))
        {
            return none();
        }

        take(BattleUnit::GhostSkillUsed);
        this->ghostSkill = kThirdFed;
        return 1;
    }

    if (mode != kHealMode && mode != kAttackMode)
    {
        return none();
    }

    const u32 wanted = mode == kHealMode ? kHealKind : kAttackKinds;
    std::array<i32, kLookedFed> qualifies = {};

    for (int at = kFirstFed; at < kLookedFed; ++at)
    {
        const i32 skill = this->ghostRecord->skills[static_cast<std::size_t>(at)];

        if (skill == 0)
        {
            continue;
        }

        this->unk2B24 = ObjectRecord{};
        database.LoadObject(this->unk2B24, static_cast<u16>(skill));
        Database::FreeDescription(this->unk2B24);

        if ((this->unk2B24.data.kind & wanted) != 0)
        {
            qualifies[static_cast<std::size_t>(at)] = 1;
        }
    }

    if (qualifies[0] == 1 && qualifies[1] == 1)
    {
        this->ghostSkill = static_cast<u16>(services.random.Below(kLookedFed) | kChosenSkill);
    }
    else
    {
        for (int at = 0; at < kLookedFed; ++at)
        {
            if (qualifies[static_cast<std::size_t>(at)] == 1)
            {
                this->ghostSkill = static_cast<u16>(at | kChosenSkill);
                break;
            }
        }
    }

    if ((this->ghostSkill & kChosenSkill) == 0)
    {
        this->ghostSkill = 0;
        return none();
    }

    const u16 index = this->ghostSkill & kSkillIndex;

    if (!this->LoadGhostSkill(database, this->ghostRecord->skills[index]))
    {
        return none();
    }

    take(mode == kHealMode ? BattleUnit::GhostHealUsed : BattleUnit::GhostAttackUsed);
    this->ghostSkill = index;
    return 1;
}

// swd3e.exe 0x00486B60: the ghost takes the player's order when its gauge fills: 1 a fed skill when a half roll lets it
// act; 2 rests (GhostRests) and gains spirit; 3 an attack skill; 4 keeps by its master; 5, 6 and 8 an attack skill when
// a half roll lets it; 7 marks GhostOrder7.
void BattleRole::TakeGhostOrder(BattleServices& services, Database& database)
{
    switch (this->ghostOrder)
    {
        case kSkillOrder:
            this->ghostActs = RollHalf(services);

            if (this->ghostActs == 1)
            {
                this->ChooseGhostSkill(services, database);
            }

            break;

        case kRestOrder:
            this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::GhostRests);
            this->ghostActs = 0;
            this->RestoreGhostSpirit(services);
            break;

        case kAttackOrder:
            this->ghostActs = 1;
            this->ChooseGhostSkillOf(services, database, kAttackMode);
            break;

        case kGuardOrder:
            this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::GhostGuards);
            break;

        case kFifthOrder:
        case kSixthOrder:
        case kEighthOrder:
            this->ghostActs = RollHalf(services);

            if (this->ghostActs == 1)
            {
                this->ChooseGhostSkillOf(services, database, kAttackMode);
            }

            break;

        case kSeventhOrder:
            this->unk2BDA = static_cast<u16>(this->unk2BDA | BattleUnit::GhostOrder7);
            break;

        default:
            break;
    }
}

// swd3e.exe 0x00486C40: the ghost's command of the turn: after a rest (GhostSpent, taken back) none (0); when it acts
// with the spirit for its chosen fed skill, the skill's script and act (`act`), its name, the skill command 2 with
// UnkCommandFlag8000 and the heals as SelectSkill; `kind` 1 for a skill of kind 0x1000 or 4 (1). Otherwise its attack
// (GhostAttackCommand, 0).
int BattleRole::GhostSkillCommand(Database& database, u32& act, u16& kind)
{
    std::fill_n(this->skillName.begin(), kClearedName, '\0');

    if ((this->unk2BDA & BattleUnit::GhostSpent) != 0)
    {
        this->unk2BDA = static_cast<u16>(this->unk2BDA & ~BattleUnit::GhostSpent);
        return 0;
    }

    const i32 skill = this->ghostActs == 1 ? this->ghostRecord->skills[this->ghostSkill] : 0;

    if (skill == 0 || !this->LoadGhostSkill(database, skill))
    {
        this->GhostAttackCommand(database);
        return 0;
    }

    const ObjectData& data = this->unk2B24.data;
    this->command = UnitCommand{};
    this->LoadCommand(database, data.skillScript);
    act = data.act;
    std::fill_n(this->skillName.begin(), kClearedName, '\0');
    const std::string name = ObjectName(this->unk2B24);
    const std::size_t length = std::min(name.size(), this->skillName.size() - 1);
    std::copy_n(name.begin(), length, this->skillName.begin());
    this->skillName[length] = '\0';
    this->unk29A4 = static_cast<u16>(this->unk29A4 + this->command.value);
    this->SetCommand(kSkillCommand);
    this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
    kind = 0;

    if ((data.kind & (kSkillKind | kHealKind)) != 0)
    {
        kind = 1;
    }

    if (data.useKind == 0)
    {
        this->restoreHitPoints = data.hitPointsUp;
        this->restoreSpirit = data.spiritUp;
        this->restoreStamina = data.staminaUp;
    }

    this->healPercent = 0;

    if ((data.useKind & kHealPercentUse) != 0)
    {
        this->healPercent = 1;
        this->restoreHitPoints = data.hitPointsUp;
        this->restoreSpirit = data.spiritUp;
        this->restoreStamina = data.staminaUp;
    }

    return 1;
}

// swd3e.exe 0x00486E10: the ghost's attack: its weapon's script as the command (command 1), the weak stand pose
// cleared, unk29A4 raised by the record's attack bonus; an attack of command flags 0x18 under command 1 becomes command
// 0xC8 with command B 1.
void BattleRole::GhostAttackCommand(Database& database)
{
    std::fill_n(this->skillName.begin(), kClearedName, '\0');
    this->command = UnitCommand{};
    this->LoadCommand(database, this->weapon.data.skillScript);
    this->SetCommand(kAttackCommand);
    const u16 flags = this->command.flags;
    this->unk2BDA = static_cast<u16>(this->unk2BDA & ~BattleUnit::WeakStand);
    this->unk29A4 = static_cast<u16>(this->unk29A4 + this->battleRecord->attackBonus);

    if ((flags & kWalkingFlags) != 0 && this->commandA == 1)
    {
        this->commandB = 1;
        this->commandA = kWalkCommand;
    }
}

// swd3e.exe 0x00486E90: the ghost spends `percent` of `base` less `less` of its spirit as the command's restore; with
// less than its whole but at least 40 percent of it, all it has; below that nothing (0). Command 2 with
// UnkCommandFlag8000 and GhostSpent follow (1).
int BattleRole::SpendGhostSpirit(int less, int base, int percent)
{
    const int cost = percent * base / kPercent - less;
    this->restoreHitPoints = static_cast<u16>(cost);
    RoleRecord& record = *this->battleRecord;
    const int spirit = record.spirit;

    if (spirit < cost)
    {
        if (spirit < cost * kSpendShare / kPercent)
        {
            this->restoreHitPoints = 0;
            return 0;
        }

        this->restoreHitPoints = static_cast<u16>(record.spirit);
    }

    record.spirit = static_cast<i16>(record.spirit - static_cast<i16>(this->restoreHitPoints));
    this->SetCommand(kSkillCommand);
    this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
    this->unk2BDA = static_cast<u16>((this->unk2BDA & ~BattleUnit::WeakStand) | BattleUnit::GhostSpent);
    return 1;
}

// swd3e.exe 0x00486F40: a roll against the level gap of `level` over the unit's own (the skill name's start cleared):
// 20 or more 85 in 100; 15 or more with no objects nor skill uses left 50 in 100; 10 or more below a third of its hit
// points 40 in 100; then by the 16-bit gap: 1..8 never, 9..15 10 in 100, 16..19 20 in 100.
int BattleRole::LevelGapRoll(BattleServices& services, u16 level)
{
    std::fill_n(this->skillName.begin(), kClearedName, '\0');
    const RoleRecord& record = *this->battleRecord;
    const int gap = level - record.level;

    if (gap >= kWideGap && static_cast<u16>(services.random.Below(kGapRoll)) < kWideGapChance)
    {
        return 1;
    }

    if (gap >= kEmptyGap)
    {
        const ObjectData& data = this->data->data;

        if (data.itemACount == 0 && data.itemBCount == 0 && data.itemCCount == 0 && data.skillACount == 0 &&
            data.skillBCount == 0 && static_cast<u16>(services.random.Below(kGapRoll)) < kEmptyGapChance)
        {
            return 1;
        }
    }

    if (record.hitPoints < record.maxHitPoints / kWeakShare && gap >= kWeakGap &&
        static_cast<u16>(services.random.Below(kGapRoll)) < kWeakGapChance)
    {
        return 1;
    }

    const auto wide = static_cast<u16>(gap);

    if (wide != 0 && wide <= kNearGap)
    {
        return 0;
    }

    if (wide >= kMiddleGapFrom && wide <= kMiddleGapTo &&
        static_cast<u16>(services.random.Below(kGapRoll)) < kMiddleGapChance)
    {
        return 1;
    }

    return wide >= kFarGapFrom && wide <= kFarGapTo && static_cast<u16>(services.random.Below(kGapRoll)) < kFarGapChance
               ? 1
               : 0;
}

// swd3e.exe 0x00487090: below half (`third` 0) or a third (1) of the party record's hit points
int BattleRole::LowHitPoints(int third) const
{
    const RoleRecord& record = *this->roleRecord;

    if (third == 0)
    {
        return record.hitPoints < record.maxHitPoints / kHalfShare ? 1 : 0;
    }

    return third == 1 && record.hitPoints < record.maxHitPoints / kThirdShare ? 1 : 0;
}

// swd3e.exe 0x004870F0: the role takes `element` (1..5): its battle resistance to it becomes unk38[0] (the whole when
// the battle record already holds that element, else its share by the proficiency of accessory kind 0x26), the
// resistance to the party record's former element is cleared and the party record holds the new one. A resistance
// index past the record is written past it in the original, skipped here (deviations.md).
void BattleRole::SetElement(CounterList& proficiency, i8 element)
{
    this->AccessoryProficiency(kElementAccessory, proficiency);
    RoleRecord& battle = *this->battleRecord;
    RoleRecord& party = *this->roleRecord;
    const int at = element - 1;
    const auto value = static_cast<i8>(battle.unk38[0]);
    const u8 taken = at == static_cast<i8>(battle.resistances[kBattleElementAt])
                         ? static_cast<u8>(value)
                         : static_cast<u8>(value * this->proficiency / kPercent);

    if (at >= 0 && static_cast<std::size_t>(at) < battle.resistances.size())
    {
        battle.resistances[static_cast<std::size_t>(at)] = taken;
    }

    const int former = static_cast<i8>(party.resistances[RoleRecord::ElementAt]);

    if (former >= 0 && static_cast<std::size_t>(former) < battle.resistances.size())
    {
        battle.resistances[static_cast<std::size_t>(former)] = 0;
    }

    party.resistances[RoleRecord::ElementAt] = static_cast<u8>(at);
}

// swd3e.exe 0x00487160
i16 BattleRole::SetUnk34C0(i16 value)
{
    this->unk34C0 = value;
    return value;
}

// swd3e.exe 0x00487170
i16 BattleRole::Unk34C0() const
{
    return this->unk34C0;
}

// swd3e.exe 0x00487180: below a quarter of the party record's spirit
bool BattleRole::LowSpirit() const
{
    return this->roleRecord->spirit < this->roleRecord->maxSpirit / kQuarterShare;
}

// swd3e.exe 0x004871A0: below a quarter of its stamina
bool BattleRole::LowStamina() const
{
    return this->roleRecord->stamina < this->roleRecord->maxStamina / kQuarterShare;
}

// swd3e.exe 0x004871C0: with equipment flag 0x80 slot act 10 takes act 0x17BF and the battle marks unk5772BC (1)
int BattleRole::StartSpecialAct(BattleState& battle)
{
    if ((this->equipmentFlags & BattleUnit::SpecialActEquipment) == 0)
    {
        return 0;
    }

    ActState& act = this->slotActs[kSpecialSlot];
    act.id = kSpecialAct;
    act.action = 0;
    battle.unk5772BC = 1;
    this->unk34D0 = 1;
    return 1;
}

// swd3e.exe 0x00487200: one frame of the special act (StartSpecialAct) while equipment flag 0x80 and unk34D0 are set:
// the battle marks unk5772BC and slot act 10 runs, its sound played (panned right) and its picture drawn at the unit's
// place (the stand act's sound cleared instead of the slot act's, as in the original). Once it ends the role stands up
// again: unk34D0 and unk5772BC cleared, the turn reset, the effects ended, the flag and `defeated` cleared, the
// accessory's proficiency read, the party record's hit points set to 40 percent of the maximum plus the proficiency
// percent of 90 percent of it and its Defeated bit cleared, the healing shown (HealTarget) and the act cleared.
void BattleRole::SpecialActFrame(BattleServices& services)
{
    if ((this->equipmentFlags & BattleUnit::SpecialActEquipment) == 0 || this->unk34D0 != 1)
    {
        return;
    }

    ActState& act = this->slotActs[kSpecialSlot];
    services.state.unk5772BC = 1;

    if (services.acts.Run(act) == 0)
    {
        return;
    }

    services.audio.PlaySound(act.view, services.config.soundVolume);
    services.audio.SetSoundPan(act.view, kSpecialPan);
    this->standAct.view = 0;
    this->picture = services.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (this->picture != nullptr)
    {
        smooth::Scene scene;
        smooth::ScenePainter painter(services, scene);
        painter.SetSource(this->picture->data.data());
        painter.DrawPicture(
            this->placement.x - act.x,
            this->placement.y - act.y,
            this->picture->width,
            this->picture->height,
            act.flags
        );
        scene.Show(services);
    }

    if (act.waiting != 1)
    {
        return;
    }

    this->unk34D0 = 0;
    services.state.unk5772BC = 0;
    this->ResetTurn(services);
    this->ExpireEffects(services, 1);
    this->equipmentFlags &= ~BattleUnit::SpecialActEquipment;
    this->defeated = 0;
    this->AccessoryProficiency(
        static_cast<u16>(AccessoryKind::SpecialAct),
        services.party.fame.lists[Fame::ProficiencyList]
    );
    RoleRecord& record = *this->roleRecord;
    record.hitPoints = static_cast<i16>(kSpecialBaseShare * record.maxHitPoints / kPercent);  // 0x00487326: 32 bits
    record.hitPoints = static_cast<i16>(
        record.hitPoints + this->proficiency * (kSpecialProficiencyShare * record.maxHitPoints / kPercent) / kPercent
    );
    record.condition = static_cast<u16>(record.condition & ~RoleRecord::Defeated);
    this->PushTarget(BattleUnit::HealTarget);
    this->SetDamageDigits(services, record.hitPoints);
    this->StepTargetCount(services, 1);
    act = ActState{};
}

}  // namespace swd3e
