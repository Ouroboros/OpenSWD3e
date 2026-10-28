#include "game/game.h"

#include <algorithm>
#include <type_traits>

#include "core/format.h"
#include "platform/display.h"

namespace swd3e
{

namespace
{

constexpr u16 kReleaseControl = 0x63;  // RoleActionFrame: the command A of the acting role
constexpr u16 kWalkCommand = 0xC8;
constexpr u16 kDoneCommand = 0x64;
constexpr u16 kReturnCommand = 0x12C;
constexpr u16 kChargeCommand = 0x190;
constexpr u16 kSwarmCommand = 0x192;
constexpr u16 kComboCommand = 0x194;
constexpr u16 kDrainSpellCommand = 0x195;
constexpr u16 kBeamCommand = 0x196;
constexpr u16 kLeapCommand = 0x199;
constexpr u16 kMagicCommand = 0x1F4;
constexpr int kAttackCommand = 1;  // RoleActionFrame: the command B
constexpr int kSpellCommand = 2;
constexpr int kItemCommand = 3;
constexpr int kMagicAttackCommand = 4;
constexpr int kGuardCommand = 5;
constexpr int kAbsorbCommand = 6;
constexpr int kDefeatCommand = 9;
constexpr int kFadeInCommand = 0xB;
constexpr int kFadeOutCommand = 0xC;
constexpr int kThrowCommand = 0xD;
constexpr int kCatchCommand = 0xE;
constexpr int kEscortCommand = 0xF;
constexpr int kLeaveCommand = 0x11;
constexpr int kCoinCommand = 0x16;
constexpr int kStealCommand = 0x17;
constexpr int kPoseCommand = 0x18;
constexpr int kControlCommand = 0x19;
constexpr int kTimingCommand = 0x1A;
constexpr int kDirectCommand = 0x1B;
constexpr int kHideCommand = 0x1C;
constexpr int kSlowCommand = 0x1D;
constexpr int kTimerCommand = 0x1F;
constexpr int kBarrierCommand = 0x20;
constexpr int kKillCommand = 0x21;
constexpr int kRouletteCommand = 0x25;
constexpr int kCastCommand26 = 0x26;  // casts at the role itself (kCast26, kCast27)
constexpr int kCastCommand27 = 0x27;
constexpr int kMeterCommand = 0x28;
constexpr int kObjectFormCommand = 0x29;
constexpr int kSealCommand = 0x50;
constexpr i32 kCast26 = 0x17BB;
constexpr i32 kCast27 = 0x17BF;
constexpr i32 kMeterCast = 0x1776;         // command 0x28: the meter shown
constexpr i32 kMeterHitCast = 0x1C9D;      // and its hit
constexpr int kSealCategory = 6;           // the item category command 0x50 uses up
constexpr i16 kAbsorbFailedCause = 0xF;    // the enrage cause of a failed taking
constexpr i32 kMusicFloor = 5;             // the lowest level the battle music sinks to while a role casts
constexpr int kSpellEnrageEvent = 2;       // CountEnrageEvent of a spell on an enemy
constexpr u8 kCastDefeatPose = 0x10;       // the defeat pose of a cast with ActFlag15
constexpr i32 kRetreatedStatus = 0x2000;   // a role that retreated
constexpr i32 kGhostAloneStatus = 0x5008;  // the talisman ghost when role 0 retreated
constexpr u16 kRouletteSound = 0xBA;
constexpr int kRouletteX = 0x168;
constexpr int kRouletteY = 0x64;
constexpr u16 kRouletteTarget = 0x2389;
constexpr u16 kRouletteTreasure = 0x25;  // the treasure kind of the roulette (AddTreasureExperience)
constexpr u16 kRoulettePoints = 5;
constexpr i32 kRouletteSetUp = 0x40000000;  // BattleState::unk577088 while the roulette runs
constexpr i32 kRouletteStopped = 0x20000000;
constexpr u32 kRouletteFrames = 0xFFFFFF;
constexpr u32 kRouletteRest = 0xF;
constexpr i16 kTreasureFrames = 0x28;
constexpr std::u8string_view kTreasureGrown = u8"龍颿法寶升級";  // 0x004BF388
constexpr int kListedCategory = 4;         // the item category a direct attack uses its object from (UseSelectedItem)
constexpr i32 kKillBoomerangAct = 0x1C87;  // swd3e: the act command 0x21 throws (BoomerangFrame)
constexpr int kSelfDamageCommand = 0x22;
constexpr int kSelfSpiritCommand = 0x23;
constexpr int kSelfStaminaCommand = 0x24;
constexpr int kAfterAction = 0x12C;
constexpr std::size_t kSpecialPoints = 1;  // RoleBattleBlock::unk00 [1] (kBattlesFought of battle_turn.cpp)
constexpr std::size_t kNextSpecial = 3;    // the next special skill (kNoSpecial: none), as in battle_turn.cpp
constexpr i32 kNoSpecial = -1;
constexpr i32 kHitPoints = 2;
constexpr i32 kDefeatPoints = 5;
constexpr i16 kDarkTint = -12;
constexpr i32 kEndCheckState = 0x63;
constexpr i32 kCraftState = 0x62;
constexpr int kSlotRecordEnemy = 2;
constexpr u16 kVampireDrain = 8;  // BattleUnit::DrainKind
constexpr u16 kVampireRing = 0x38;
constexpr u16 kVampireStep = 5;
constexpr u16 kVampireKind = 0x38;  // the accessory kind of 吸血鬼指環 (AccessoryKind::LifeDrain)
constexpr i32 kTenthPercent = 10;
constexpr i32 kPercent = 100;
constexpr u16 kVampireTarget = 0x237D;
constexpr u16 kVampireDelay = 8;
constexpr i32 kComboStarted = 1;  // BattleState::unk53BF2C
constexpr i32 kComboRunning = 2;
constexpr u16 kComboCount = 0x7FFF;
constexpr i32 kComboMark = 4;  // marks set on every other unit while the combo runs
constexpr i32 kComboStill = 0x40;
constexpr u16 kComboBand = 1;
constexpr u16 kAbsorbWait = 0x28;   // RoleActionFrame command 6: the frames shown after a failed absorb
constexpr u16 kAbsorbShown = 0x1E;  // and after a successful one
constexpr u16 kAbsorbMessage = 2;
constexpr i16 kKnownAbsorbs = 0x14;
constexpr u16 kSucceededSound = 0x117;
constexpr u16 kFailedSound = 0x116;
constexpr int kNoticeX = 0x118;
constexpr int kNoticeY = 0xA;
constexpr i16 kAbsorbFrames = 0x28;
constexpr i16 kStealFrames = 0x32;
constexpr i16 kStealFailFrames = 0x1E;
constexpr i16 kControlFrames = 0x32;
constexpr int kNoticeFlags = BattleMessage::Frame | BattleMessage::Centered;
constexpr int kRetreatFlags = BattleMessage::BlackBar | BattleMessage::Centered;
constexpr std::u8string_view kAbsorbSucceeded = u8"收化成功";             // 0x004BF364
constexpr std::u8string_view kAbsorbFailed = u8"收化失敗";                // 0x004BF358
constexpr std::u8string_view kStoleFormat = u8"成功偷到[%s]!!";           // 0x004BF348
constexpr std::u8string_view kNothingToSteal = u8"敵人身上無任何物品!!";  // 0x004BF330
constexpr std::u8string_view kStealFailed = u8"偷竊失敗!!";               // 0x004BF324
constexpr std::u8string_view kControlled = u8"控制成功!!";                // 0x004BF318
constexpr std::u8string_view kControlFailed = u8"控制失敗!!";             // 0x004BF30C
constexpr u16 kNothingStolen = 0x61A8;
constexpr int kQueueSlots = 8;
constexpr int kSpellCast = 0x1965;  // the cast acts of the accessory commands (CastAt)
constexpr int kSlowCast = 0x1791;
constexpr int kSelfCast = 0x17B7;
constexpr u16 kHideAccessory = 0x1C;
constexpr u16 kSlowAccessory = 0x1D;
constexpr u16 kBarrierAccessory = 0x20;
constexpr u16 kKillAccessory = 0x21;
constexpr u32 kBuffTurnsScale = 4;
constexpr u8 kBuffTurnsBase = 2;
constexpr u8 kKilledPose = 7;
constexpr u16 kPoseHit = 0x8000;  // BattleRole::AttackPose: the cast act once the act strikes
constexpr u16 kPoseAct = 0x7FFF;
constexpr u16 kPoseDone = 2;
constexpr i32 kMaxHitDamage = 0x270F;
constexpr u16 kOrderPrepared = 0x8000;  // BattleState::enemyOrders
constexpr u16 kOrderSpell = 0x4000;
constexpr int kTimingX = 0x140;  // RoleActionFrame command 0x1A: the timing bar
constexpr int kTimingY = 0xC8;
constexpr i32 kTimingPressed = 1;  // BattleState::unk577080
constexpr i32 kTimingTaken = 0x8000;
constexpr i32 kTimingKept = 0xFFF;
constexpr u16 kTimingDone = 0x8000;
constexpr u16 kTimingSound = 0x2C;
constexpr u32 kLowWord = 0xFFFF;
constexpr u32 kHighWord = 0xFFFF0000;
constexpr u16 kTimingStart = 1;
constexpr i32 kTimerStarted = static_cast<i32>(0x80000000u);  // BattleState::unk577088
constexpr i32 kTimerPressed = 1;
constexpr int kTimerSeconds = 5;
constexpr int kTimerFlag = 0x4B;
constexpr int kTimerDefeatFlag = 0x4A;
constexpr u16 kTimerSound = 0x2E;
constexpr i32 kNoEscortPanel = 4;  // BattleState::panelFlags: no escort comes on
constexpr u16 kMaxEscorts = 2;
constexpr i8 kListGlowOn = static_cast<i8>(0x80);
constexpr u16 kNoUnit = 0xFFFF;
constexpr u16 kAnswersStart = 2;
constexpr i16 kFadeWord = 8;
constexpr int kRippleGrid = 0xC8;
constexpr u16 kGaugeFullSound = 0x2C;                     // RoleFrame
constexpr u32 kUnaimedEffect = SkillScript::UnkAilment4;  // an AI role does not aim at a unit with this effect
constexpr u32 kAimsAtRole = 0x80000000u;                  // BattleState::aiTarget of the ghost aiming at a role
constexpr int kGhostSkillOfLow = 1;                       // ChooseGhostSkillOf for a role low on hit points
constexpr int kGhostSkillOfMost = 3;                      // and at the most hit enemy
constexpr int kGhostHealSpirit = 0x46;                    // SpendGhostSpirit: the percent of the spirit it spends
constexpr i32 kGhostHealCast = 0x17B7;
constexpr u32 kGhostSkillRoll = 0x32;     // a skill at the most hit enemy above this of 100
constexpr u16 kGhostRoleSkill = 1;        // GhostSkillCommand: the skill is for a role
constexpr i32 kOrderedTurn = 2;           // BattleState::turnTaken of a role ordered to act at once
constexpr int kGhostOutStatus1 = 0x4000;  // the talisman ghost out with role 0 (cleared when it comes back)
constexpr int kGhostOutStatus2 = 8;
constexpr int kGhostOutStatus3 = 0x1000;
constexpr int kGhostBackStatus = 0x200;
constexpr i16 kLowSpiritCause = 0x19;  // EnrageEnemies after a role's action
constexpr i16 kLowStaminaCause = 0x1A;
constexpr i16 kRetreatFailedCause = 0x16;
constexpr i16 kSkillNameFrames = 0x23;
constexpr u16 kRetreatLevel = 0xC8;
constexpr u16 kRetreatAsked = 0x4000;  // BattleState::unk577204
constexpr u16 kRetreating = 0x8000;
constexpr u16 kRetreatRoles = 0x7FFF;
constexpr i16 kRetreatFrames = 0x1E;
constexpr std::u8string_view kRetreatFailed = u8"撤退失敗!!";  // 0x004BF37C
constexpr std::u8string_view kRetreated = u8"撤退成功!!";      // 0x004BF370
constexpr u16 kRetreatFailedSound = 0x8C;
constexpr i32 kRetreatedState = 0x68;
constexpr i32 kFacingPanel = 0x80;
constexpr u16 kTalkCountdown = 0x7FFF;

}  // namespace

// swd3e.exe 0x0045BEA0: one frame of the command of role `role` on unit `target` (an enemy, or a role while
// unk575F24); 1 once the command is over (at once for a defeated role, for an enemy target past the enemies unless
// the hit falls on a whole side, and for a role without commands). Command A 0xC8 walks to the target, the larger
// ones are the special attacks (RoleSpecialFrame); the others, and command B, run RoleCommandFrame (command B alone
// takes none of them: 0).
// swd3e.exe 0x004A96E0 (EH unwind: the new ripple, freed by its unique_ptr)
// swd3e.exe 0x004A96EB (EH handler of 0x0045BEA0)
int Game::RoleActionFrame(int role, int target)
{
    BattleState& battle = this->battleState;
    BattleRole& unit = this->battleRoles[static_cast<std::size_t>(role)];
    const u16 command = unit.GetCommandA();

    if (unit.IsDefeated())
    {
        return 1;
    }

    if (battle.unk575F24 == 0 && battle.wholeSide == 0 && target >= static_cast<i32>(battle.enemyCount))
    {
        return 1;
    }

    if (command == 0)
    {
        return unit.GetCommandB() == 0 ? 1 : 0;
    }

    if (command == kWalkCommand)
    {
        battle.slideUnit = target;
        unit.WalkToTarget(this->battleServices, this->AimedUnit(target));
        return 0;
    }

    if (command > kWalkCommand)
    {
        return this->RoleSpecialFrame(unit, role, target, command);
    }

    return this->RoleCommandFrame(unit, role, target, command);
}

// The unit a role's command aims at (inline in 0x0045BEA0): role `target` while unk575F24, otherwise enemy `target`.
BattleUnit& Game::AimedUnit(int target)
{
    const auto at = static_cast<std::size_t>(target);

    if (this->battleState.unk575F24 != 0)
    {
        return this->battleRoles[at];
    }

    return this->battleEnemies[at];
}

// The special-move points of the party role of battle role `role` (inline in 0x0045BEA0).
void Game::AddSpecialPoints(int role, i32 points)
{
    const auto partyRole = static_cast<std::size_t>(this->battleState.partyRoles[static_cast<std::size_t>(role)]);
    this->party.battleBlocks[partyRole].values[kSpecialPoints] += points;
}

// The special-move points of a spell (inline in 0x0045BEA0): only while the party role has a next special.
void Game::AddSpellPoints(int role, i32 points)
{
    const auto partyRole = static_cast<std::size_t>(this->battleState.partyRoles[static_cast<std::size_t>(role)]);
    RoleBattleBlock& block = this->party.battleBlocks[partyRole];

    if (block.values[kNextSpecial] != kNoSpecial)
    {
        block.values[kSpecialPoints] += points;
    }
}

// A hit of a role landed on enemy `target` (inline in 0x0045BEA0).
void Game::RoleHitLanded(int target)
{
    BattleState& battle = this->battleState;
    battle.hitLanded = 1;
    battle.lastEnemyHit = static_cast<u16>(target);
    battle.slotTargets[static_cast<std::size_t>(target)] = target;
    battle.unk5772BC = 0;
}

// Unit `target` defeated by a role (inline in 0x0045BEA0): recorded for the removal, the frame flashed white.
void Game::RoleDefeated(int target)
{
    BattleState& battle = this->battleState;
    battle.unk4BF1CA = static_cast<u16>(target);
    battle.unk513664[static_cast<std::size_t>(target)] = target;
    battle.flashing = 1;
    this->FlashFrameWhite();
}

// The battle music a level lower, down to kMusicFloor, each frame a spell or an item runs (inline in 0x0045BEA0).
void Game::LowerBattleMusic()
{
    BattleState& battle = this->battleState;
    const i32 level = battle.musicLevel;

    if (level > kMusicFloor)
    {
        --battle.musicLevel;
        this->audio.Queue().SetVolume(MusicQueue::MaxVolume * (level - 1) / Config::MaxVolume);
    }
}

// The battle music back at the options' volume (inline in 0x0045BEA0).
void Game::RestoreBattleMusic()
{
    this->battleState.musicLevel = this->config.musicVolume;
    this->audio.Queue().SetVolume(MusicQueue::MaxVolume * this->config.musicVolume / Config::MaxVolume);
}

// The field darkened by tint steps of kDarkTint (inline in 0x0045BEA0).
void Game::DarkenForCommand()
{
    BattleState& battle = this->battleState;
    battle.darkRed = kDarkTint;
    battle.darkGreen = kDarkTint;
    battle.darkBlue = kDarkTint;
    battle.unk577290 = 1;
}

// The darkening of a command undone (inline in 0x0045BEA0).
void Game::EndCommandDarkening()
{
    BattleState& battle = this->battleState;
    battle.unk577290 = 0;
    battle.darkRed = 0;
    battle.darkGreen = 0;
    battle.darkBlue = 0;
    battle.darkenUnit = -1;
    battle.pushFrames = 0;
}

// The command A above 0xC8 of RoleActionFrame (inline in the original): 300 returns home; the special attacks 0x190
// (charge), 0x192 (swarm, the field darkened by tint), 0x194 (the combo: the other units marked still while it runs),
// 0x195 (drain spell), 0x196 (beam), 0x199 (leap) and 0x1F4 (magic, the music lowered while it runs) land their hit
// (two special points, five more for a defeat; the magic only while the role has a next special and without damage)
// and end with command 0x12C and no fury (0x199 is over then).
int Game::RoleSpecialFrame(BattleRole& unit, int role, int target, u16 command)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

    // The damage of a landed special attack (inline in the original).
    auto damage = [this, &battle, &unit, &enemy, role, target]
    {
        if (battle.unk576084 == 0 && enemy.Damage(this->battleServices, battle.hitDamage, 0, 0) == 1)
        {
            battle.hitDamage = -1;
            this->RoleDefeated(target);
            unit.SetCommand(kAfterAction);
            this->AddSpecialPoints(role, kDefeatPoints);
        }

        battle.hitDamage = 0;
        unit.AddFury(this->battleServices, 0);
        unit.SetCommand(kAfterAction);
        return 0;
    };

    auto landed = [this, role, target, &damage](int done)
    {
        if (done != 1)
        {
            return 0;
        }

        this->RoleHitLanded(target);
        this->AddSpecialPoints(role, kHitPoints);
        return damage();
    };

    switch (command)
    {
        case kReturnCommand:
            if (unit.ReturnHome(services, this->AimedUnit(target), -1, 0) != 1)
            {
                return 0;
            }

            battle.darkenUnit = -1;
            return 1;

        case kChargeCommand:
            return landed(unit.ChargeAttackFrame(services, enemy, 0));

        case kSwarmCommand:
            this->DarkenForCommand();
            battle.darkenByTint = 1;

            if (unit.SwarmSpellFrame(services, enemy) != 1)
            {
                return 0;
            }

            this->AddSpecialPoints(role, kHitPoints);
            return damage();

        case kComboCommand:
            return this->ComboFrame(unit, role, target);

        case kDrainSpellCommand:
            return landed(this->DrainSpellFrame(unit, enemy));

        case kBeamCommand:
            return landed(unit.BeamSpellFrame(services, enemy));

        case kLeapCommand:
            if (unit.LeapSpellFrame(services, enemy) != 1)
            {
                return 0;
            }

            this->RoleHitLanded(target);
            this->AddSpecialPoints(role, kHitPoints);

            if (battle.unk576084 == 0 && enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
            {
                this->RoleDefeated(target);
                this->AddSpecialPoints(role, kDefeatPoints);
            }

            battle.hitDamage = 0;
            battle.darkenUnit = -1;
            unit.SetCommand(kAfterAction);
            unit.AddFury(services, 0);
            return 1;

        case kMagicCommand:
            this->LowerBattleMusic();

            if (unit.MagicFrame(services, enemy) != 1)
            {
                return 0;
            }

            this->RestoreBattleMusic();
            this->AddSpellPoints(role, kHitPoints);
            this->RoleHitLanded(target);
            battle.hitDamage = 0;
            unit.AddFury(services, 0);
            unit.SetCommand(kAfterAction);
            return 0;

        default:
            return 0;
    }
}

// The combo of RoleSpecialFrame (inline in the original): its first frame marks every other role not guarding and
// every enemy still (kComboMark, kComboStill), the next opens the grey band (grayBandState 2, grayBand 1); then the
// magic attack lands like the other special attacks and the band and every mark are cleared again.
int Game::ComboFrame(BattleRole& unit, int role, int target)
{
    BattleState& battle = this->battleState;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

    if ((battle.grayBandState & kComboCount) == 0)
    {
        battle.grayBandState = kComboStarted;

        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            BattleRole& other = this->battleRoles[index];

            if (static_cast<int>(index) != role && !other.IsGuarding())
            {
                other.SetStatus(kComboMark);
                other.SetStatus(kComboStill);
            }
        }

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            this->battleEnemies[index].SetStatus(kComboStill);
            this->battleEnemies[index].SetStatus(kComboMark);
        }
    }

    if ((battle.grayBandState & kComboCount) == kComboStarted)
    {
        battle.grayBandState = kComboRunning;
        battle.grayBand = kComboBand;
        battle.grayBandShown = 1;
    }

    if ((battle.grayBandState & kComboCount) != kComboRunning || this->MagicAttackFrame(unit, enemy) != 1)
    {
        return 0;
    }

    this->RoleHitLanded(target);
    this->AddSpecialPoints(role, kHitPoints);

    if (battle.unk576084 == 0 && enemy.Damage(this->battleServices, battle.hitDamage, 0, 0) == 1)
    {
        battle.hitDamage = -1;
        this->RoleDefeated(target);
        unit.SetCommand(kAfterAction);
        this->AddSpecialPoints(role, kDefeatPoints);
    }

    battle.hitDamage = 0;
    battle.grayBandState = 0;
    battle.grayBand = 0;
    battle.grayBandShown = 0;
    unit.AddFury(this->battleServices, 0);
    unit.SetCommand(kAfterAction);

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        if (static_cast<int>(index) != role)
        {
            this->battleRoles[index].ClearStatus(kComboMark);
            this->battleRoles[index].ClearStatus(kComboStill);
        }
    }

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        this->battleEnemies[index].ClearStatus(kComboMark);
        this->battleEnemies[index].ClearStatus(kComboStill);
    }

    return 0;
}

// The commands up to 100 of RoleActionFrame (inline in the original), by command A (command B when A is 0 and B set:
// none of these, so 0).
int Game::RoleCommandFrame(BattleRole& unit, int role, int target, u16 command)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(target);

    switch (command)
    {
        case kAttackCommand:
            return this->RoleAttackFrame(unit, target);

        case kSpellCommand:
            return this->RoleSpellFrame(unit, role, target);

        case kItemCommand:
            return this->RoleItemFrame(unit, role, target);

        case kMagicAttackCommand:
        {
            BattleEnemy& enemy = this->battleEnemies[at];
            this->LowerBattleMusic();

            if (this->MagicAttackFrame(unit, enemy) != 1)
            {
                return 0;
            }

            this->AddSpellPoints(role, kHitPoints);
            this->RoleHitLanded(target);

            if (battle.unk576084 == 0 && enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
            {
                battle.hitDamage = -1;
                this->RoleDefeated(target);
                unit.SetCommand(kAfterAction);
                this->AddSpellPoints(role, kDefeatPoints);
            }

            this->RestoreBattleMusic();
            battle.hitDamage = 0;
            unit.AddFury(services, 0);
            unit.SetCommand(kAfterAction);
            return 0;
        }

        case kGuardCommand:
            battle.darkenUnit = -1;
            return 1;

        case kAbsorbCommand:
            return this->AbsorbFrame(unit, target, false);

        case kSealCommand:
            return this->AbsorbFrame(unit, target, true);

        case kDefeatCommand:
        {
            BattleEnemy& enemy = this->battleEnemies[at];

            if (enemy.DefeatFrame(services) != 1)
            {
                return 0;
            }

            enemy.SetCommand(0);
            battle.RemoveSlotRecord(target);

            if (static_cast<u32>(target) < Party::Roles)
            {
                ++battle.enemiesGone;
            }

            return 1;
        }

        case kFadeInCommand:
        case kFadeOutCommand:
        {
            const bool fadeIn = command == kFadeInCommand;

            if ((fadeIn ? unit.DrawFadeIn(services, unit) : unit.DrawFadeOut(services)) != 1)
            {
                return 0;
            }

            unit.SetGuarding(fadeIn ? 0 : 1);
            unit.SetWord2A0A(kFadeWord);
            battle.enemyFaded = 1;
            battle.darkenUnit = -1;
            return 1;
        }

        case kThrowCommand:
            return this->ThrowTalismanFrame(unit, role, target);

        case kCatchCommand:
            if (battle.pushFrames == 0)
            {
                this->DarkenForCommand();
                battle.pushFrames = 1;
                this->DarkenByTint();
                battle.darkLevel = 1;
            }

            if (unit.CatchAct(services, this->battleEnemies[at]) != 1)
            {
                return 0;
            }

            battle.result = kCraftState;
            this->EndCommandDarkening();
            battle.undarkening = 1;
            return 1;

        case kEscortCommand:
            return this->SummonEscortFrame(unit, role);

        case kLeaveCommand:
            return this->RetreatCommandFrame(unit, role);

        case kCoinCommand:
            return this->CoinTossFrame(role);

        case kStealCommand:
            return this->StealFrame(unit, target);

        case kPoseCommand:
            return this->PoseSweepFrame(unit);

        case kControlCommand:
            return this->ControlFrame(unit, role, target);

        case kTimingCommand:
            return this->TimingAttackFrame(unit, target);

        case kDirectCommand:
        {
            BattleEnemy& enemy = this->battleEnemies[at];

            if (unit.DirectAttack(services, enemy) != 1)
            {
                return 0;
            }

            battle.usedUpItem = unit.UseSelectedItem(kListedCategory);

            if (battle.unk576084 == 0 && enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
            {
                this->RoleDefeated(target);
            }

            battle.hitDamage = 0;
            battle.darkenUnit = -1;
            return 1;
        }

        case kHideCommand:
        case kSlowCommand:
        case kBarrierCommand:
            return this->AccessoryBuffFrame(unit, target, command);

        case kTimerCommand:
            return this->TimerAttackFrame(unit, target);

        case kKillCommand:
        {
            BattleEnemy& enemy = this->battleEnemies[at];

            if (unit.BoomerangFrame(services, enemy, kKillBoomerangAct) != 1)
            {
                return 0;
            }

            battle.darkenUnit = -1;
            battle.lastDefeatedObject = enemy.Data();

            // Without an object the original reads through a null pointer: taken as immune.
            if (battle.lastDefeatedObject == nullptr ||
                (battle.lastDefeatedObject->data.kind & ObjectData::UnkKind20) != 0)
            {
                return 1;
            }

            if (RollChance(this->random, unit.AccessoryProficiency(kKillAccessory, this->ProficiencyList())))
            {
                enemy.SetDefeatPose(kKilledPose);
                enemy.StartDefeat(1);
                this->RoleDefeated(target);
            }

            return 1;
        }

        case kSelfDamageCommand:
        case kSelfSpiritCommand:
        case kSelfStaminaCommand:
            return this->SelfHitFrame(unit, command);

        case kRouletteCommand:
            return this->RouletteFrame(unit, target);

        case kCastCommand26:
        case kCastCommand27:
            if (unit.CastAt(services, unit, command == kCastCommand26 ? kCast26 : kCast27) != 1)
            {
                return 0;
            }

            battle.darkenUnit = -1;
            return 1;

        case kMeterCommand:
            return this->MeterAttackFrame(unit, target);

        case kObjectFormCommand:
        {
            BattleRole& other = this->battleRoles[at];

            if (unit.ZoomEntranceFrame(services, other, battle.usedUpItem) != 1)
            {
                return 0;
            }

            other.RaiseFlag2BC0Bit17();
            other.TakeObjectForm(this->database, battle.usedUpItem);
            battle.usedUpItem = 0;
            battle.darkenUnit = -1;
            battle.undarkening = 1;
            return 1;
        }

        case kReleaseControl:
        {
            BattleEnemy& controlled =
                this->battleEnemies[static_cast<std::size_t>(static_cast<i16>(battle.controlledEnemy))];
            controlled.SetHeld(0);
            controlled.SetFacing(0);
            battle.controlCursor = 1;
            battle.controlledEnemy = kNoUnit;
            battle.talkRole = kNoUnit;
            battle.controlRows = kAnswersStart;
            battle.darkenUnit = -1;
            return 1;
        }

        case kDoneCommand:
            return 1;

        default:
            return 0;
    }
}

// The proficiency list of the party (Fame::ProficiencyList, 0x004D1F58).
CounterList& Game::ProficiencyList()
{
    return this->party.fame.lists[Fame::ProficiencyList];
}

// Command 1 of RoleActionFrame (inline in the original): the attack lands on its target (an enemy, or a role while
// unk575F24). The attacker drains after the hit (on the enemy of the target's index even for a role target, kept); a
// vampire ring (DrainKind 8) turns a tenth plus its proficiency of the rest into healing of the attacker (on an enemy
// target, with its digits; on a role target the share is only computed); its counters step by 5. An enemy target
// slides the units back (SlideBack). The command goes on with 0x12C.
int Game::RoleAttackFrame(BattleRole& unit, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(target);
    BattleEnemy& enemy = this->battleEnemies[at];
    BattleUnit& aimed = this->AimedUnit(target);

    if (unit.AttackFrame(services, aimed, 0) != 1)
    {
        return 0;
    }

    this->RoleHitLanded(target);

    if (battle.unk575F24 == 0)
    {
        battle.slideMode = BattleState::SlideBack;
    }

    // A defeat keeps the damage (no -1 as after the special attacks).
    if (battle.unk576084 == 0 && aimed.Damage(services, battle.hitDamage, 0, 0) == 1)
    {
        this->RoleDefeated(target);
        unit.SetCommand(kAfterAction);
    }

    if (battle.unk575F24 != 0)
    {
        this->ApplyDrain(battle.hitDamage, unit, enemy);
    }

    if (unit.DrainKind() == kVampireDrain)
    {
        const i32 share = static_cast<u16>(unit.AccessoryProficiency(kVampireRing, this->ProficiencyList()));
        const i32 tenth = battle.hitDamage * kTenthPercent / kPercent;

        if (battle.unk575F24 != 0)
        {
            battle.hitDamage = tenth + battle.hitDamage * share / kPercent;
            battle.lastDamage = 0;
        }
        else
        {
            battle.lastDamage = 0;
            battle.hitDamage = tenth + (battle.hitDamage - tenth) * share / kPercent;
            unit.PushTarget(kVampireTarget);
            battle.hitDamage = -battle.hitDamage;
            unit.SetDamageDigits(services, battle.hitDamage);
            unit.Damage(services, battle.hitDamage, 0, 0);
            unit.PushTargetAlt(kVampireDelay);
            unit.StepTargetCount(services, 1);
        }

        unit.StepAccessoryCounters(this->ProficiencyList(), kVampireKind, kVampireStep);
    }

    if (battle.unk575F24 == 0)
    {
        this->ApplyDrain(battle.hitDamage, unit, enemy);
    }

    battle.hitDamage = 0;
    battle.staminaDamage = 0;
    battle.spiritDamage = 0;
    unit.SetCommand(kAfterAction);
    return 0;
}

// The first frame of a spell or an item of a role (inline in 0x0045BEA0): SpellCast and the sweep reset.
void Game::StartRoleCast(BattleRole& unit)
{
    BattleState& battle = this->battleState;
    battle.spellStage = static_cast<u16>(battle.spellStage | BattleState::SpellCast);
    battle.sweepGap = unit.GetWordDA6();
    battle.slotTargets.fill(-1);
    battle.sweptUnits = 1;
    battle.slotDamage.fill(0);
}

// Every enemy standing defeated by a cast with ActFlag15 (inline in 0x0045BEA0): pose 0x10, unk5772F8 set.
void Game::DefeatAllByCast(BattleRole& unit)
{
    BattleState& battle = this->battleState;

    if (unit.ActFlag15() != 1)
    {
        return;
    }

    battle.unk5772F8 = 1;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        BattleEnemy& enemy = this->battleEnemies[index];

        if (!enemy.IsDefeated())
        {
            battle.unk513664[index] = static_cast<i32>(index);
            enemy.StartDefeat(1);
            enemy.SetDefeatPose(kCastDefeatPose);
        }
    }
}

// Command 2 of RoleActionFrame (inline in the original): the spell. Its first frame slides the units to an enemy
// target, sets up the cast (StartRoleCast; with ActFlag13 the ripple battle) and the targets: on the roles a whole
// side counts at most the roles, a single defeated role is retargeted unless the command only refreshes; on the
// enemies the target (or with a whole side every enemy standing, counted for the sweep) counts enrage event 2 and
// takes the command's turns; ActFlag15 defeats the enemies. The music sinks while the spell runs; once it is over
// the music comes back, the units slide back, the turns and damage are cleared and, unless the resistance panel
// shows, the ripple goes and the role goes on with 0x12C (retreating if it asked to).
int Game::RoleSpellFrame(BattleRole& unit, int role, int target)
{
    BattleState& battle = this->battleState;

    if ((battle.spellStage & BattleState::SpellCast) == 0)
    {
        if (battle.unk575F24 == 0)
        {
            battle.slideMode = BattleState::SlideToUnit;
            battle.slideUnit = target;
        }

        this->StartRoleCast(unit);

        if (unit.ActFlag13() == 1)
        {
            battle.rippleHits = 1;
            battle.ripple =
                std::make_unique<ScriptObject>(Display::Width, Display::Height, 0, 0, kRippleGrid, kRippleGrid);
        }

        if (battle.unk575F24 != 0)
        {
            if (battle.wholeSide == 1)
            {
                if (battle.sweepTotal > static_cast<i32>(battle.roleCount))
                {
                    battle.sweepTotal = static_cast<i32>(battle.roleCount);
                }
            }
            else if (unit.CommandRefreshOnly() == 0 && this->battleRoles[static_cast<std::size_t>(target)].IsDefeated())
            {
                unit.Retarget(static_cast<i16>(role));
            }
        }
        else
        {
            if (battle.sweepTotal > static_cast<i32>(battle.enemyCount))
            {
                battle.sweepTotal = static_cast<i32>(battle.enemyCount);
            }

            this->battleEnemies[static_cast<std::size_t>(target)].CountEnrageEvent(kSpellEnrageEvent);
            battle.enemyCommandTurns[0] = unit.CommandTurns();

            if (battle.wholeSide == 1)
            {
                battle.sweepTotal = 0;

                for (u32 index = 0; index < battle.enemyCount; ++index)
                {
                    BattleEnemy& enemy = this->battleEnemies[index];

                    if (!enemy.IsDefeated())
                    {
                        ++battle.sweepTotal;
                        enemy.CountEnrageEvent(kSpellEnrageEvent);
                        battle.enemyCommandTurns[index] = unit.CommandTurns();
                    }
                }
            }
        }

        this->DefeatAllByCast(unit);
    }

    if ((battle.spellStage & BattleState::SpellOver) == 0)
    {
        this->LowerBattleMusic();
        return 0;
    }

    this->RestoreBattleMusic();
    battle.slideMode = BattleState::SlideBack;
    // The original calls an empty function on every role and every enemy here.
    battle.enemyCommandTurns.fill(0);
    battle.hitDamage = 0;
    battle.spiritDamage = 0;
    battle.staminaDamage = 0;

    if ((battle.panelFlags & BattleState::ResistancePanel) != 0)
    {
        return 0;
    }

    if (battle.rippleHits == 1)
    {
        battle.ripple.reset();
    }

    battle.rippleHits = 0;

    if (battle.unk5772C8 == 0)
    {
        battle.undarkening = 1;
    }

    unit.SetCommand(kAfterAction);
    this->TryRetreat(role);
    return 0;
}

// Command 3 of RoleActionFrame (inline in the original): the item. Its first frame sets up the cast (StartRoleCast;
// on the enemies the units slide to the target) with sweepTotal at most the units of the target side, and ActFlag15
// defeats the enemies. The music sinks every frame; once the item is over the music comes back, the item is used up
// (usedUpItem), and the role goes on with 0x12C (retreating if it asked to).
int Game::RoleItemFrame(BattleRole& unit, int role, int target)
{
    BattleState& battle = this->battleState;

    if ((battle.spellStage & BattleState::SpellCast) == 0)
    {
        this->StartRoleCast(unit);
        i32 units = static_cast<i32>(battle.roleCount);

        if (battle.unk575F24 == 0)
        {
            units = static_cast<i32>(battle.enemyCount);
            battle.slideMode = BattleState::SlideToUnit;
            battle.slideUnit = target;
        }

        if (battle.sweepTotal > units)
        {
            battle.sweepTotal = units;
        }

        this->DefeatAllByCast(unit);
    }

    this->LowerBattleMusic();

    if ((battle.spellStage & BattleState::SpellOver) == 0)
    {
        return 0;
    }

    this->RestoreBattleMusic();
    battle.slideMode = 0;
    battle.usedUpItem = unit.UseSelectedItem(battle.listRole);
    battle.hitDamage = 0;
    battle.spiritDamage = 0;
    battle.staminaDamage = 0;

    if (battle.unk5772C8 == 0)
    {
        battle.undarkening = 1;
    }

    unit.SetCommand(kAfterAction);
    this->TryRetreat(role);
    return 0;
}

// Commands 6 and 0x50 of RoleActionFrame (inline in the original): the taking (收化) of the enemy. Command 6 takes
// an enemy taken kKnownAbsorbs times before (the list 0x004D3458) or one CanAbsorb allows; command 0x50 (`sealed`)
// first uses up the selected item of category 6 and takes only what CanAbsorb allows. Otherwise it fails at once.
// Taken, the field darkens until the enemy leaves the slot records and its object joins the inventory; the message
// (收化成功, the enemy falling; 收化失敗, enrage cause 0xF) shows after the wait, and for command 0x50 with every
// enemy gone the battle ends.
int Game::AbsorbFrame(BattleRole& unit, int target, bool sealed)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

    if (battle.pushFrames > 1)
    {
        if (--battle.pushFrames != kAbsorbMessage)
        {
            return 0;
        }

        if (battle.absorbSucceeded == 1)
        {
            enemy.StartDefeat(1);
            enemy.SetDefeatPoseFixed(0);
            this->audio.PlaySound(kSucceededSound, this->config.soundVolume);
            this->battleMessages.Add(kNoticeX, kNoticeY, kAbsorbFrames, kAbsorbSucceeded, kNoticeFlags);
        }
        else
        {
            this->audio.PlaySound(kFailedSound, this->config.soundVolume);
            this->battleMessages.Add(kNoticeX, kNoticeY, kAbsorbFrames, kAbsorbFailed, kNoticeFlags);
            enemy.EnrageCheck(services, kAbsorbFailedCause, 0);
        }

        this->EndCommandDarkening();
        battle.unk4BF1CA = kNoUnit;
        battle.unk577330 = 0;

        if (battle.unk5772C8 == 0)
        {
            battle.undarkening = 1;
        }

        if (sealed && static_cast<i32>(battle.enemiesGone) - battle.unk5771EA >= static_cast<i32>(battle.enemyCount))
        {
            battle.autoBattle = 0;
            battle.result = kEndCheckState;
        }

        return 1;
    }

    if (battle.pushFrames == 0)
    {
        if (sealed)
        {
            battle.usedUpItem = unit.UseSelectedItem(kSealCategory);
        }
        else if (
            static_cast<u16>(FindCounterValue(this->party.fame.lists[Fame::FameList], enemy.ActId())) >= kKnownAbsorbs
        )
        {
            battle.unk577330 = 1;
        }

        if (unit.CanAbsorb(services, enemy) == 1)
        {
            battle.unk577330 = 1;
        }
        else if (battle.unk577330 != 1)
        {
            battle.absorbSucceeded = 0;
            battle.pushFrames = kAbsorbWait;
            return 0;
        }

        unit.StartAbsorb(services, enemy);
        battle.absorbSucceeded = 1;
        enemy.SetValueAAC(1);
        this->DarkenForCommand();
        battle.pushFrames = 1;
        this->DarkenByTint();
    }

    if (unit.AbsorbFrame(services) == 1)
    {
        battle.RemoveSlotRecord(target);
        battle.pushFrames = kAbsorbShown;

        if (sealed)
        {
            battle.unk4BF1CA = static_cast<u16>(target);
        }

        AddOneItem(
            this->party.inventory,
            battle.enemyPlaces[static_cast<std::size_t>(target)].actId,
            1,
            this->database
        );
        this->CureHeldBy(target);
        battle.enemyHits[static_cast<std::size_t>(target)] = 0;
    }

    return 0;
}

// Command 0xD of RoleActionFrame (inline in the original): the talisman thrown (the field darkened) gives the enemy
// an empty ailment and queues the enemy (1-based) in the role's row of actionQueue (its first free slot of the first
// eight; none: nothing).
int Game::ThrowTalismanFrame(BattleRole& unit, int role, int target)
{
    BattleState& battle = this->battleState;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

    if (battle.pushFrames == 0)
    {
        this->DarkenForCommand();
        battle.pushFrames = 1;
        this->DarkenByTint();
    }

    if (unit.ThrowAct(this->battleServices, enemy) != 1)
    {
        return 0;
    }

    battle.accessoryEffect = SkillScript{};
    battle.darkenUnit = -1;
    enemy.AddAilment(this->battleServices, battle.accessoryEffect);
    this->EndCommandDarkening();
    battle.undarkening = 1;
    const auto row = static_cast<std::size_t>(role * BattleState::QueueColumns);

    for (int slot = 0; slot < kQueueSlots; ++slot)
    {
        i16& queued = battle.actionQueue[row + static_cast<std::size_t>(slot)];

        if (queued == 0)
        {
            queued = static_cast<i16>(target + 1);
            break;
        }
    }

    return 1;
}

// Command 0xF of RoleActionFrame (inline in the original): the escort of the role (escortCalled) comes on (one more
// role unit, at most kMaxEscorts, unless panel flag 4 holds it back; one escort gone fewer): set up at its place,
// facing the enemies; the role glows (the field darkened) and the escort takes its place in the order.
int Game::SummonEscortFrame(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    u16& escortOf = battle.escortCalled[static_cast<std::size_t>(role + BattleState::FirstRoleUnit)];

    if (battle.pushFrames == 0)
    {
        if (battle.unk5771E9 != 0)
        {
            --battle.unk5771E9;
        }

        if ((battle.panelFlags & kNoEscortPanel) == 0 && battle.escortCount < kMaxEscorts)
        {
            ++battle.escortCount;
            ++battle.roleCount;
        }

        battle.callingEscort = escortOf;
        BattleRole& escort = this->battleRoles[battle.callingEscort];
        escort.Reset(services);
        escort.SetB00(1);
        escort.ClearIdleAction();

        if (battle.mirrored == 0)
        {
            escort.SetFacing(1);
        }

        escort.SetupEscort(battle.rolePlaces[battle.callingEscort], this->battleServices, this->database);
        battle.pushFrames = 1;
        battle.unk5770B0 = 0;
        battle.unk5770B4 = 0;
    }

    this->DarkenForCommand();
    this->DarkenByTint();
    const UnitPlacement& place = battle.rolePlaces[battle.callingEscort];

    if (unit.GlowAct(services, place.x, place.y) != 1)
    {
        return 0;
    }

    battle.RemoveSlotRecord(battle.callingEscort + BattleState::FirstRoleUnit);
    battle.panelFlags &= ~kNoEscortPanel;
    battle.unk5376B8[battle.callingEscort] = 0;
    battle.callingEscort = 0;
    escortOf = 0;
    battle.listGlow = kListGlowOn;
    battle.guardCursor = 0;
    battle.darkenUnit = -1;
    battle.pushFrames = 0;
    this->MeasureUnits();
    battle.SortDrawOrder();
    return 1;
}

// Command 0x11 of RoleActionFrame (inline in the original): the retreat. The role steps back; then its level is
// rolled against the highest level of the enemies standing (levelUpFrames): lost, 撤退失敗!! and the command is over.
// Won, an escort (AutoControl) steps away and leaves (the last ones gone: every escort's place), a role steps away
// held and guarding (role 0 takes the talisman ghost out with it: status 0x5008) and counts as defeated; with every
// role gone the battle ends (0x68), and 撤退成功!! shows.
int Game::RetreatCommandFrame(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if (battle.unk577204 == 0)
    {
        if (unit.StepBack(services, 0) != 1)
        {
            return 0;
        }

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            BattleEnemy& enemy = this->battleEnemies[index];

            if (enemy.IsDefeated())
            {
                continue;
            }

            battle.lastDefeatedObject = enemy.Data();

            // Without an object the original reads through a null pointer: taken as level 0.
            if (battle.lastDefeatedObject != nullptr && battle.lastDefeatedObject->data.level > battle.levelUpFrames)
            {
                battle.levelUpFrames = battle.lastDefeatedObject->data.level;
            }
        }

        if (unit.BeatsLevel(services, battle.levelUpFrames) != 1)
        {
            this->battleMessages.Add(kNoticeX, kNoticeY, kRetreatFrames, kRetreatFailed, kRetreatFlags);
            this->audio.PlaySound(kRetreatFailedSound, this->config.soundVolume);
            return 1;
        }

        battle.unk577204 = 1;
    }
    else if (battle.unk577204 != 1)
    {
        return 0;
    }

    if (unit.AutoControl() == 1)
    {
        if (unit.StepBack(services, 1) != 1)
        {
            return 0;
        }

        const u16 escorts = battle.escortCount;
        const auto roles = static_cast<i32>(battle.roleCount);
        battle.unk577204 = 0;
        const i32 row = role + escorts - roles;

        // Rows outside the escorts write the words beside givenBack in the original; none here.
        if (row >= 0 && static_cast<std::size_t>(row + 1) < battle.givenBack.size())
        {
            battle.givenBack[static_cast<std::size_t>(row + 1)] = 0;
        }

        ++battle.unk5771E9;
        battle.rolePlaces[static_cast<std::size_t>(role)].actId = 0;

        if (battle.unk5771E9 < escorts)
        {
            if (row == 1)
            {
                battle.escortCount = static_cast<u16>(escorts - 1);
                battle.roleCount = static_cast<u32>(roles - 1);
                battle.rolePlaces[static_cast<std::size_t>(roles - 1)] = UnitPlacement{};
            }
        }
        else
        {
            // The original clears the places from the end of the list down, before it when there are fewer.
            for (u16 cleared = 0; cleared < escorts && cleared < static_cast<u16>(roles); ++cleared)
            {
                battle.rolePlaces[static_cast<std::size_t>(roles - 1 - cleared)] = UnitPlacement{};
            }

            battle.escortCount = 0;
            battle.roleCount = static_cast<u32>(roles - escorts);
            battle.unk5771E9 = 0;
        }

        return 1;
    }

    if (unit.StepBack(services, 1) != 1)
    {
        return 0;
    }

    unit.SetAF8(1);
    unit.SetStatus(kRetreatedStatus);
    unit.SetGuarding(1);

    if (role == 0)
    {
        battle.unk577370 = 1;
        this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)].SetStatus(kGhostAloneStatus);
        battle.RemoveSlotRecord(battle.ghostSlot + BattleState::FirstRoleUnit);
    }

    ++battle.defeatedRoles;

    if (static_cast<u32>(battle.defeatedRoles) >= battle.roleCount - battle.unk5770D4 - battle.escortCount)
    {
        battle.result = kRetreatedState;
    }

    battle.unk577204 = 0;
    this->battleMessages.Add(kNoticeX, kNoticeY, kRetreatFrames, kRetreated, kRetreatFlags);
    return 1;
}

// Command 0x16 of RoleActionFrame (inline in the original): the coin toss. Its first frame holds every enemy standing
// (SetUnk2BC0Bit19), darkens the field by role `role` and starts the coin (coinTossing; BattleFrame draws it and closes
// the grey band, grayBandState 2, once it lands, which marks the spell cast); the music sinks meanwhile. Once the
// spell is over the music comes back, the enemies are let go and the command is over.
int Game::CoinTossFrame(int role)
{
    BattleState& battle = this->battleState;
    this->LowerBattleMusic();

    auto holdStanding = [this, &battle](int on)
    {
        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            if (!this->battleEnemies[index].IsDefeated())
            {
                this->battleEnemies[index].SetUnk2BC0Bit19(on);
            }
        }
    };

    if (battle.grayBandState == 0)
    {
        holdStanding(1);
        this->DarkenForCommand();
        battle.darkenUnit = static_cast<i16>(role + BattleState::FirstRoleUnit);
        this->DarkenByTint();
        battle.grayBandState = kComboStarted;
        battle.coinTossing = 1;
    }
    else if (battle.grayBandState == kComboRunning)
    {
        battle.spellStage = static_cast<u16>(battle.spellStage | BattleState::SpellCast | BattleState::UnkSpell400);
    }

    if ((battle.spellStage & BattleState::SpellOver) == 0)
    {
        return 0;
    }

    this->RestoreBattleMusic();
    holdStanding(0);
    battle.spellStage = 0;
    battle.darkenUnit = -1;
    battle.grayBandState = 0;
    battle.undarkening = 1;
    battle.unk577290 = 0;
    return 1;
}

// Command 0x17 of RoleActionFrame (inline in the original): once posed at the enemy the role steals: an object shows
// 成功偷到[name]!! (in noticeText) and joins the inventory; nothing shows 敵人身上無任何物品!! (kNothingStolen) or
// 偷竊失敗!!.
int Game::StealFrame(BattleRole& unit, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

    if (unit.PoseAtTarget(services, enemy) != 1)
    {
        return 0;
    }

    const u16 stolen = unit.Steal(services, enemy);

    if (stolen != 0 && stolen < kNothingStolen)
    {
        battle.noticeText.fill('\0');
        this->database.LoadObject(battle.lastObject, stolen);
        this->database.FreeDescription(battle.lastObject);
        const std::string name = ObjectName(battle.lastObject);
        const std::string line = WinFormat(kStoleFormat, {std::string_view(name)});
        const std::size_t length = std::min(line.size(), battle.noticeText.size() - 1);
        std::copy_n(line.begin(), length, battle.noticeText.begin());
        this->audio.PlaySound(kSucceededSound, this->config.soundVolume);
        this->battleMessages.Add(
            kNoticeX,
            kNoticeY,
            kStealFrames,
            std::span<const char>(battle.noticeText.data(), length),
            kNoticeFlags
        );
        AddOneItem(this->party.inventory, stolen, 1, this->database);
    }
    else
    {
        this->audio.PlaySound(kFailedSound, this->config.soundVolume);
        this->battleMessages.Add(
            kNoticeX,
            kNoticeY,
            kStealFailFrames,
            stolen == kNothingStolen ? kNothingToSteal : kStealFailed,
            kNoticeFlags
        );
    }

    unit.SetCommand(kAfterAction);
    return 0;
}

// Command 0x18 of RoleActionFrame (inline in the original): the attack pose; once it strikes (kPoseHit) every enemy
// standing takes the role's hit (the damage running total of the frame, capped per hit, its digits, a defeat flashing
// white) and its hit act becomes enemiesHitAct; the command is over once the pose is (kPoseDone).
int Game::PoseSweepFrame(BattleRole& unit)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    battle.lastRoleHit = unit.AttackPose(services);

    if ((battle.lastRoleHit & kPoseHit) != 0)
    {
        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            BattleEnemy& enemy = this->battleEnemies[index];

            if (enemy.IsDefeated())
            {
                continue;
            }

            battle.unk575F38[index] = static_cast<i32>(index);
            battle.lastDamage = unit.HitDamage(services, enemy, battle.spiritDamage, battle.staminaDamage);

            if (battle.lastDamage >= kMaxHitDamage)
            {
                battle.lastDamage = kMaxHitDamage;
            }

            battle.hitDamage += battle.lastDamage;
            enemy.RaiseFlag2BC0(this->battleState);
            enemy.SetDamageDigits(services, battle.hitDamage);
            enemy.StepTargetCount(services, 1);

            if (battle.unk576084 == 0 && enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
            {
                this->RoleDefeated(static_cast<int>(index));
            }

            battle.hitDamage = 0;
        }

        battle.enemiesHitAct = battle.lastRoleHit & kPoseAct;
    }

    if (battle.lastRoleHit == kPoseDone)
    {
        battle.darkenUnit = -1;
        return 1;
    }

    battle.lastRoleHit = 0;
    return 0;
}

// Command 0x19 of RoleActionFrame (inline in the original): without a controlled enemy the role tries to control the
// enemy (控制成功!!: it faces the party, its turn resets and it leaves the slot records; or 控制失敗!!). With one the
// role orders it: its order (enemyOrders) is a prepared command, the chosen skill (orderedSkill) or the chosen object
// (unk577018), its act in the enemy's enemyHitActs, aimed at the enemy actingEnemy (1-based in aimTarget), and it is
// queued to act.
int Game::ControlFrame(BattleRole& unit, int role, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if (battle.controlledEnemy == kNoUnit)
    {
        BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

        if (unit.CanTarget(enemy) != 1)
        {
            this->battleMessages.Add(kNoticeX, kNoticeY, kControlFrames, kControlFailed, kNoticeFlags);
            return 1;
        }

        this->battleMessages.Add(kNoticeX, kNoticeY, kControlFrames, kControlled, kNoticeFlags);
        enemy.SetFacing(1);
        battle.controlledEnemy = static_cast<u16>(target);
        battle.talkRole = static_cast<u16>(role);
        enemy.ResetTurn(services);
        battle.RemoveSlotRecord(target);
        battle.darkenUnit = -1;
        return 1;
    }

    const auto controlled = static_cast<std::size_t>(static_cast<i16>(battle.controlledEnemy));
    BattleEnemy& enemy = this->battleEnemies[controlled];
    u16& order = battle.enemyOrders[controlled];
    i32& hitAct = battle.enemyHitActs[controlled];
    battle.enemyAimsAtEnemies = 1;
    battle.aimTarget = battle.actingEnemy + 1;
    order = kOrderPrepared;

    if (battle.orderedSkill != 0)
    {
        order = kOrderSpell;
        enemy.LoadSkillAction(this->database, static_cast<u16>(battle.orderedSkill), hitAct);
        battle.orderedSkill = 0;
    }

    if (battle.unk577018 != 0)
    {
        order = kOrderPrepared;
        u32 act = static_cast<u32>(hitAct);
        enemy.LoadEnemyCommand(this->database, 1, act);
        hitAct = static_cast<i32>(act);
    }

    order = static_cast<u16>(order | static_cast<u16>(battle.aimTarget - 1));
    battle.AddSlotRecord(kSlotRecordEnemy, static_cast<i16>(battle.controlledEnemy));
    battle.darkenUnit = -1;
    return 1;
}

// Command 0x1A of RoleActionFrame (inline in the original): the timing bar runs while the role holds still
// (kComboStill once the needle starts); a press (unk577080) takes the timing: a hit with the needle on the mark
// (timingHit) sounds and frees the role, otherwise the next attack repeats (numberFont's low word). Each attack that
// lands holds the role again; a defeat holds the enemy and ends the bar (kTimingDone), as does the bar's end without a
// press.
int Game::TimingAttackFrame(BattleRole& unit, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleHud& hud = this->battleHud;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

    auto setLow = [](auto& value, u32 low)
    {
        using Value = std::remove_reference_t<decltype(value)>;
        value = static_cast<Value>((static_cast<u32>(value) & kHighWord) | low);
    };

    auto end = [&unit, &battle, &hud, &setLow]
    {
        unit.ClearStatus(kComboStill);
        unit.SetCommand(kAfterAction);
        hud.timingHit = 0;
        battle.unk577328 = 0;
        setLow(hud.numberFont, 0);
        setLow(hud.timingCounter, 0);
    };

    battle.unk577328 = 1;

    if ((static_cast<u32>(hud.timingCounter) & kLowWord) == kTimingStart)
    {
        unit.SetStatus(kComboStill);
    }

    if (hud.DrawTimingBar(kTimingX, kTimingY) == 1 && (battle.unk577080 & kTimingTaken) == 0)
    {
        end();
        return 0;
    }

    if ((battle.unk577080 & kTimingPressed) != 0)
    {
        battle.unk577080 = kTimingTaken;

        if ((hud.numberFont & kLowWord) == 0)
        {
            if (hud.timingHit != 0)
            {
                hud.timingHit = 0;
                this->audio.PlaySound(kTimingSound, this->config.soundVolume);
                unit.ClearStatus(kComboStill);
            }
            else
            {
                setLow(hud.numberFont, 1);
                hud.timingHit = 0;
            }
        }
    }

    if (unit.AttackFrame(services, enemy, 0) != 1)
    {
        return 0;
    }

    battle.unk577080 &= kTimingKept;
    this->RoleHitLanded(target);
    battle.unk577328 = 0;

    if (battle.unk576084 == 0 && enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
    {
        battle.hitDamage = -1;
        battle.unk4BF1CA = static_cast<u16>(target);
        battle.unk513664[static_cast<std::size_t>(target)] = target;
        battle.flashing = 1;
        enemy.SetHeld(1);
        this->FlashFrameWhite();
        setLow(hud.timingCounter, kTimingDone);
    }

    battle.hitDamage = 0;
    unit.SetStatus(kComboStill);

    if ((static_cast<u32>(hud.timingCounter) & kTimingDone) != 0)
    {
        end();
    }

    return 0;
}

// Commands 0x1C, 0x1D and 0x20 of RoleActionFrame (inline in the original): the accessory buff cast (on the role,
// 0x1D on the enemy) with turns of 2 plus 4 per hundred of the accessory's proficiency: 0x1C makes the role invisible,
// 0x20 sets its barrier, 0x1D lowers the enemy's attack (hit).
int Game::AccessoryBuffFrame(BattleRole& unit, int target, int command)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];
    SkillScript& skill = battle.accessoryEffect;

    auto buff = [this, &unit, &skill](u16 accessory, u32 bits)
    {
        skill = SkillScript{};
        const u16 proficiency = static_cast<u16>(unit.AccessoryProficiency(accessory, this->ProficiencyList()));
        skill.statusBits = bits;
        skill.turns = static_cast<u8>(kBuffTurnsScale * proficiency / kPercent + kBuffTurnsBase);
    };

    if (command == kSlowCommand)
    {
        if (unit.CastAt(services, enemy, kSlowCast) != 1)
        {
            return 0;
        }

        buff(kSlowAccessory, SkillScript::AttackDown);
        enemy.AddBuff(services, skill);
        enemy.RaiseFlag2BC0(this->battleState);
        battle.darkenUnit = -1;
        return 1;
    }

    if (unit.CastAt(services, unit, command == kHideCommand ? kSpellCast : kSelfCast) != 1)
    {
        return 0;
    }

    if (command == kHideCommand)
    {
        buff(kHideAccessory, SkillScript::Invisible);
    }
    else
    {
        buff(kBarrierAccessory, SkillScript::Barrier);
    }

    unit.AddBuff(services, skill);
    battle.darkenUnit = -1;
    return 1;
}

// Command 0x1F of RoleActionFrame (inline in the original): a five-second timer (StartTimer) runs over the enemy
// (its place in unk577246); game flag 0x4B (the timer's end) ends the command; each press (unk577088) hits the enemy
// with the role's hit (its digits) and a defeat clears game flag 0x4A and ends it.
int Game::TimerAttackFrame(BattleRole& unit, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];
    battle.result = 0;

    if (battle.unk577088 == 0)
    {
        battle.unk577088 = kTimerStarted;
        this->StartTimer(0, kTimerSeconds, 0, 1);
        u16 x = 0;
        u16 y = 0;
        enemy.GetDrawPosition(x, y);
        battle.unk577246 = static_cast<i16>(x);
        battle.unk577248 = static_cast<i16>(y);
        this->battleHud.ResetEffectAt();
    }

    if (this->state.GameFlag(kTimerFlag))
    {
        this->state.ClearGameFlag(kTimerFlag);
        battle.unk577088 = 0;
        battle.unk57724C = 0;
        battle.darkenUnit = -1;
        return 1;
    }

    if ((battle.unk577088 & kTimerPressed) == 0)
    {
        return 0;
    }

    this->audio.PlaySound(kTimerSound, this->config.soundVolume);
    this->battleHud.ResetEffectAt();
    battle.unk577088 = kTimerStarted;
    battle.unk57724C = 1;
    battle.hitDamage = unit.HitDamage(services, enemy, battle.spiritDamage, battle.staminaDamage);
    enemy.RaiseFlag2BC0(this->battleState);
    enemy.SetDamageDigits(services, battle.hitDamage);
    enemy.StepTargetCount(services, 1);

    if (battle.unk576084 == 0 && enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
    {
        this->state.ClearGameFlag(kTimerDefeatFlag);
        this->RoleDefeated(target);
        battle.hitDamage = 0;
        battle.unk577088 = 0;
        battle.darkenUnit = -1;
        return 1;
    }

    battle.hitDamage = 0;
    return 0;
}

// Commands 0x22..0x24 of RoleActionFrame (inline in the original): the role casts at itself and takes its own hit as
// damage (0x22), spirit (0x23) or stamina (0x24), with the digits.
int Game::SelfHitFrame(BattleRole& unit, int command)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if (unit.CastAt(services, unit, kSelfCast) != 1)
    {
        return 0;
    }

    battle.lastDamage = unit.HitDamage(services, unit, battle.spiritDamage, battle.staminaDamage);

    switch (command)
    {
        case kSelfDamageCommand:
            unit.SetDamageDigits(services, battle.lastDamage);
            unit.StepTargetCount(services, 1);
            unit.Damage(services, battle.lastDamage, 0, 0);
            battle.lastDamage = 0;
            break;

        case kSelfSpiritCommand:
            unit.SetDamageDigits(services, battle.spiritDamage);
            unit.StepTargetCount(services, 1);
            unit.Damage(services, 0, battle.spiritDamage, 0);
            battle.spiritDamage = 0;
            break;

        default:
            unit.SetDamageDigits(services, battle.staminaDamage);
            unit.StepTargetCount(services, 1);
            unit.Damage(services, 0, 0, battle.staminaDamage);
            battle.staminaDamage = 0;
            break;
    }

    battle.darkenUnit = -1;
    return 1;
}

// Command 0x25 of RoleActionFrame (inline in the original): the roulette of the treasure (龍颿法寶). While the role
// casts (CastAt with act 0) nothing more happens; then the roulette is set up by the treasure's level and spins until a
// press stops it (kRouletteSound) and it comes to rest; kRouletteRest frames later its number hits the enemy (a defeat
// flashing white), the treasure gains 5 plus the prize in experience (龍颿法寶升級 when it grows) and the command is
// over.
int Game::RouletteFrame(BattleRole& unit, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleHud& hud = this->battleHud;
    battle.result = 0;

    if (unit.CastAt(services, unit, 0) == 0)
    {
        return 0;
    }

    if ((battle.unk577088 & kRouletteSetUp) == 0)
    {
        hud.SetupRoulette(static_cast<i16>(unit.TreasureLevel(this->ProficiencyList())));
        battle.unk577088 |= kRouletteSetUp;
    }

    if ((battle.unk577088 & kRouletteStopped) == 0)
    {
        if ((battle.unk577088 & kTimerPressed) != 0)
        {
            this->audio.PlaySound(kRouletteSound, this->config.soundVolume);
            battle.unk577088 = kRouletteSetUp;

            if (hud.DrawRoulette(kRouletteX, kRouletteY, battle.hitDamage, 1) == 1)
            {
                battle.unk577088 |= kRouletteStopped;
            }
        }
        else
        {
            hud.DrawRoulette(kRouletteX, kRouletteY, battle.hitDamage, 0);
        }

        return 0;
    }

    hud.DrawRoulette(kRouletteX, kRouletteY, battle.hitDamage, 0);

    if ((static_cast<u32>(++battle.unk577088) & kRouletteFrames) < kRouletteRest)
    {
        return 0;
    }

    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];
    enemy.PushTarget(kRouletteTarget);
    enemy.RaiseFlag2BC0(this->battleState);
    enemy.SetDamageDigits(services, battle.hitDamage);
    enemy.StepTargetCount(services, 1);

    if (battle.unk576084 == 0 && enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
    {
        this->RoleDefeated(target);
        battle.darkenUnit = -1;
    }

    const u16 prize = hud.RouletteBonus();

    if (unit.AddTreasureExperience(
            this->ProficiencyList(),
            kRouletteTreasure,
            static_cast<u16>(prize + kRoulettePoints)
        ) == 1)
    {
        this->battleMessages.Add(kNoticeX, kNoticeY, kTreasureFrames, kTreasureGrown, kRetreatFlags);
    }

    hud.ResetRoulette();
    battle.unk577088 = 0;
    battle.hitDamage = 0;
    return 1;
}

// Command 0x28 of RoleActionFrame (inline in the original): the meter. Without it shown the role casts at itself
// (act 0x1776) and shows it; with it the role casts at the enemy (act 0x1C9D) and hits it (capped, its digits, a
// defeat flashing white even while unk576084), and the meter goes.
int Game::MeterAttackFrame(BattleRole& unit, int target)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleHud& hud = this->battleHud;

    if (hud.meterShown != 0)
    {
        BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(target)];

        if (unit.CastAt(services, enemy, kMeterHitCast) != 1)
        {
            return 0;
        }

        battle.hitDamage = unit.HitDamage(services, enemy, battle.spiritDamage, battle.staminaDamage);

        if (battle.hitDamage >= kMaxHitDamage)
        {
            battle.hitDamage = kMaxHitDamage;
        }

        enemy.RaiseFlag2BC0(this->battleState);
        enemy.SetDamageDigits(services, battle.hitDamage);
        enemy.StepTargetCount(services, 1);

        if (enemy.Damage(services, battle.hitDamage, 0, 0) == 1)
        {
            this->RoleDefeated(target);
        }

        hud.ResetMeter();
        hud.meterShown = 0;
        battle.hitDamage = 0;
    }
    else
    {
        if (unit.CastAt(services, unit, kMeterCast) != 1)
        {
            return 0;
        }

        hud.meterShown = 1;
    }

    battle.darkenUnit = -1;
    return 1;
}

// swd3e.exe 0x0045FA80: one frame of battle role `role`; 1 always. The role of the start pose only runs its pose
// (Pose36Frame) until it is over. Otherwise the role is drawn at rest (its acts holding while the field is darkened
// with word 2A74, or while an enemy enrages). While the fight talk lets the battle run and no unit acts its effects
// tick and its gauge steps; a full gauge regenerates it and either opens the menu for a party role or lets an AI role
// (an escort, a guest, the talisman ghost) choose its command (RoleGaugeFull). A role whose turn comes is slotted or
// acts at once (RoleTakeTurn); its order is taken (RoleOrderFrame), the acting role acts (RoleActFrame) and a retreat
// steps the roles back (RoleRetreatFrame). A defeat is taken out.
int Game::RoleFrame(int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if ((battle.startPoseRole & BattleState::StartPoseDone) == 0 && battle.startPoseRole == role)
    {
        battle.unk5772BC = 1;

        if (this->battleRoles[battle.startPoseRole].Pose36Frame(services) == 1)
        {
            battle.unk5772BC = 0;
            battle.startPoseRole = static_cast<u8>(battle.startPoseRole | BattleState::StartPoseDone);
        }

        return 1;
    }

    BattleRole& unit = this->battleRoles[static_cast<std::size_t>(role)];
    const bool holding =
        (unit.GetWord2A74() != 0 && (battle.unk577290 == 1 || battle.unk577294 == 1)) || battle.enraging == 1;
    unit.IdleFrame(services, holding ? 1 : 0);

    if (this->fightTalk.battleRuns == 1 && battle.actingUnit == -1 && battle.unk5772BC == 0 && battle.unk5772C0 == 0)
    {
        unit.TickEffects(services);

        if (unit.StepGauge(services, battle.enemyGaugeBoost) == 1)
        {
            this->RoleGaugeFull(unit, role);
        }
    }

    this->RoleTakeTurn(unit, role);

    if (unit.target == 1)
    {
        this->RoleOrderFrame(unit, role);
    }

    if (battle.actingUnit == role + BattleState::FirstRoleUnit)
    {
        this->RoleActFrame(unit, role);
    }

    if (battle.hitsAllRoles == 0 && !unit.IsDefeated() && unit.Ghost() != 1 && unit.AutoControl() != 1)
    {
        this->RoleRetreatFrame(unit, role);
    }

    unit.SpecialActFrame(services);

    if (this->RemoveDefeated(role, 1) == 1)
    {
        battle.damageShown = 0;
        battle.lastRoleDefeated = kNoUnit;
    }

    return 1;
}

// The full gauge of a role in RoleFrame (inline in the original): a party member's panel stops growing and the role
// regenerates. A party role sounds and takes the panel (currentUnit; the menu opens at once unless the battle fights
// itself). An AI role with an enemy to aim at (one standing without kUnaimedEffect) chooses its command
// (RoleAutoCommand); without one its turn resets.
void Game::RoleGaugeFull(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;

    if (unit.PartyMember() == 1)
    {
        battle.panelGrown[static_cast<std::size_t>(role)] = 0;
    }

    unit.Regenerate(this->battleServices);

    if (unit.AutoControl() != 1 && unit.GuestControl() != 1 && unit.Ghost() != 1)
    {
        this->audio.PlaySound(kGaugeFullSound, this->config.soundVolume);
        battle.unk5772C0 = 1;
        battle.currentUnit = role + BattleState::FirstRoleUnit;
        battle.unk5772BC = 1;

        if (battle.autoBattle == 0)
        {
            battle.unk577258 = 1;
            this->ConfirmKey();
        }

        return;
    }

    u32 unaimed = 0;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        BattleEnemy& enemy = this->battleEnemies[index];

        if (enemy.IsDefeated() || enemy.HasEffectBits(kUnaimedEffect) == 1)
        {
            ++unaimed;
        }
    }

    if (unaimed >= battle.enemyCount)
    {
        unit.ResetTurn(this->battleServices);
        return;
    }

    this->RoleAutoCommand(unit, role);
    battle.unk5772C0 = 1;
    battle.unk5772BC = 1;
}

// The command of an AI role with a full gauge (inline in RoleFrame): it aims (aiTarget, 1-based) at the enemy the
// ghost is locked on (ghostLockTarget), else at a random enemy standing without kUnaimedEffect. The talisman ghost's
// orders then aim it: order 5 at the weakest enemy, 6 at the strongest (both measured against the first enemy standing
// only, as in the original), a fed skill at the first role low on hit points, an attack at the first enemy without
// its effect, a heal at the first role without it (a role aim marked by kAimsAtRole), order 7 at the role lowest on
// hit points (with none low the most hit enemy, half the times with a skill of mode 3; with one low and no skill the
// ghost spends spirit on kGhostHealCast). The ghost takes the command of its skill (a role skill at a random role) and
// aims at aimTarget; an escort or a guest retreats when the enemies' mean level beats it, else chooses its command (a
// supporting one aims at itself).
void Game::RoleAutoCommand(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(role);
    PendingOrder& order = battle.pendingOrders[at];
    i32& aim = battle.aiTarget;

    auto enemyAt = [this](i32 oneBased) -> BattleEnemy&
    {
        return this->battleEnemies[static_cast<std::size_t>(oneBased - 1)];
    };

    auto roleAt = [this](i32 oneBased) -> BattleRole&
    {
        return this->battleRoles[static_cast<std::size_t>(oneBased - 1)];
    };

    // A role the ghost may aim at: standing, not an escort or the ghost, without kUnaimedEffect.
    auto aimable = [](const BattleRole& other)
    {
        return !other.IsDefeated() && other.AutoControl() != 1 && other.Ghost() != 1 &&
               other.HasEffectBits(kUnaimedEffect) != 1;
    };

    unit.SetTarget(1);
    aim = battle.ghostLockTarget;

    if (battle.ghostLockTarget == 0 || enemyAt(battle.ghostLockTarget).IsDefeated())
    {
        do
        {
            do
            {
                aim = static_cast<i32>(this->random.Below(battle.enemyCount)) + 1;
            } while (enemyAt(aim).IsDefeated());
        } while (enemyAt(aim).HasEffectBits(kUnaimedEffect) == 1);
    }

    // Order 5 aims at the enemy below the first standing one (`weaker`), 6 above it.
    auto aimByStrength = [this, &battle, &aim](bool weaker)
    {
        u32 index = 0;
        i32 first = 0;

        do
        {
            first = this->battleEnemies[index].HitPointsAndAttack();
            ++index;
        } while (this->battleEnemies[index - 1].IsDefeated());

        aim = static_cast<i32>(index);

        for (u32 other = 1; other < battle.enemyCount; ++other)
        {
            BattleEnemy& enemy = this->battleEnemies[other];

            if (enemy.IsDefeated())
            {
                continue;
            }

            const i32 strength = enemy.HitPointsAndAttack();

            if (weaker ? strength < first : strength > first)
            {
                aim = static_cast<i32>(other) + 1;
            }
        }
    };

    if (unit.GhostOrderIs5())
    {
        aimByStrength(true);
    }

    if (unit.GhostOrderIs6())
    {
        aimByStrength(false);
    }

    battle.unk57725C = 1;

    if (unit.TakeSkillUsed())
    {
        aim = 1;
        unit.SetGhostActs(0);

        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            BattleRole& other = this->battleRoles[index];

            if (aimable(other) && other.LowHitPoints(0) == 1)
            {
                unit.SetGhostActs(1);
                aim = static_cast<i32>((index + 1) | kAimsAtRole);
                break;
            }
        }
    }

    if (unit.TakeAttackUsed())
    {
        unit.SetGhostActs(0);

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            BattleEnemy& enemy = this->battleEnemies[index];

            if (!enemy.IsDefeated() && unit.HasCommandEffect(enemy) == 0)
            {
                unit.SetGhostActs(1);
                aim = static_cast<i32>(index) + 1;
                break;
            }
        }
    }

    if (unit.TakeHealUsed())
    {
        aim = 1;
        unit.SetGhostActs(0);

        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            BattleRole& other = this->battleRoles[index];

            if (aimable(other) && unit.HasCommandEffect(other) == 0)
            {
                unit.SetGhostActs(1);
                aim = static_cast<i32>((index + 1) | kAimsAtRole);
                break;
            }
        }
    }

    if (unit.TakeOrder7() == 1)
    {
        unit.SetGhostActs(0);
        std::size_t index = 0;
        i32 lowest = 0;

        // With no role to aim at the original reads past the roles (deviations.md): it stops at the last one.
        do
        {
            lowest = this->battleRoles[index].HitPointShare(kPercent);
            ++index;
        } while (!aimable(this->battleRoles[index - 1]) && index < this->battleRoles.size());

        aim = static_cast<i32>(index);

        for (u32 other = 0; other < battle.roleCount; ++other)
        {
            BattleRole& candidate = this->battleRoles[other];

            if (aimable(candidate) && candidate.LowHitPoints(1) == 1)
            {
                if (candidate.HitPointShare(kPercent) < lowest)
                {
                    aim = static_cast<i32>(other) + 1;
                }

                unit.SetGhostActs(1);
                aim = static_cast<i32>(static_cast<u32>(aim) | kAimsAtRole);
            }
        }

        if (aim < 0 && unit.ChooseGhostSkillOf(services, this->database, kGhostSkillOfLow) == 0)
        {
            i32 hitPoints = 0;
            i32 maximum = 0;
            roleAt(static_cast<i32>(static_cast<u32>(aim) & ~kAimsAtRole)).GetHitPoints(hitPoints, maximum);

            if (unit.SpendGhostSpirit(hitPoints, maximum, kGhostHealSpirit) == 1)
            {
                battle.unk50D88C[at] = kGhostHealCast;
                order.unk00 = 1;
            }
            else
            {
                aim = 0;
            }
        }

        if (aim >= 0)
        {
            aim = battle.MostHitEnemy();

            while (enemyAt(aim).IsDefeated())
            {
                aim = static_cast<i32>(this->random.Below(battle.enemyCount)) + 1;
            }

            if (this->random.Below(kPercent) > kGhostSkillRoll)
            {
                unit.ChooseGhostSkillOf(services, this->database, kGhostSkillOfMost);
                unit.SetGhostActs(1);
            }
        }
    }

    if (unit.Ghost() == 1)
    {
        u32 act = static_cast<u32>(battle.unk50D88C[at]);
        u16 kind = 0;
        const int chosen = unit.GhostSkillCommand(this->database, act, kind);
        battle.unk50D88C[at] = static_cast<i32>(act);

        if (chosen == 1)
        {
            if (unit.ActFlag1() == 1)
            {
                order.unk08 = 1;
            }

            if (unit.ActFlag0() == 1)
            {
                order.unk04 = 1;
            }

            order.unk00 = 0;

            if (kind == kGhostRoleSkill)
            {
                order.unk00 = 1;

                if (aim >= 0)
                {
                    do
                    {
                        do
                        {
                            aim = static_cast<i32>(this->random.Below(battle.roleCount)) + 1;
                        } while (roleAt(aim).IsDefeated());
                    } while (roleAt(aim).AutoControl() == 1 || roleAt(aim).Ghost() == 1 ||

                             roleAt(aim).HasEffectBits(kUnaimedEffect) == 1);
                }
            }
        }

        aim = static_cast<i32>(static_cast<u32>(aim) & ~kAimsAtRole);
        battle.aimTarget = aim;
        return;
    }

    u32 levels = 0;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        // Without an object the original reads through a null pointer: taken as level 0.
        const ObjectRecord* data = this->battleEnemies[index].Data();
        levels += data != nullptr ? data->data.level : 0;
    }

    if (unit.LevelGapRoll(services, static_cast<u16>(levels / battle.enemyCount)) == 1)
    {
        unit.SetCommand(kLeaveCommand);
        return;
    }

    u32 act = static_cast<u32>(battle.unk50D88C[at]);
    const int chosen = unit.ChooseCommand(services, this->database, act);
    battle.unk50D88C[at] = static_cast<i32>(act);

    if (chosen != 1)
    {
        return;
    }

    if (unit.ActFlag1() == 1)
    {
        order.unk08 = 1;
    }

    if (unit.ActFlag0() == 1)
    {
        order.unk04 = 1;
    }

    if (unit.CommandSupports() == 1)
    {
        aim = role + 1;
        order.unk00 = 1;
    }
}

// The turn of a role in RoleFrame (inline in the original): a role not slotted, not held, ready (GetValueAB4) while no
// unit acts, the battle goes on, no role has the panel and the fight talk lets it run, once its second gauge drains,
// takes the turn (its turn resets when the enemy being taken is the last one or the battle is to end with nothing
// pending). A role ordered to act (turnTaken 2) acts at once when no slot record waits (its pending order becomes the
// acting state), otherwise it is slotted first; any other is slotted last.
void Game::RoleTakeTurn(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(role);
    const int self = role + BattleState::FirstRoleUnit;

    if (battle.unk5376B8[at] != 0 || unit.IsHeld() != 0 || unit.GetValueAB4() != 1 || battle.actingUnit != -1 ||
        battle.result != 0 || battle.currentUnit != 0 || this->fightTalk.battleRuns != 1 ||
        unit.DrainSecondGauge() != 1)
    {
        return;
    }

    if (battle.unk4BF1CA != kNoUnit && static_cast<i32>(battle.enemyCount) - battle.enemiesGone <= 1)
    {
        unit.ResetTurn(services);
    }

    if (battle.result == kEndCheckState && battle.pendingActionRow == 0 && battle.pendingAction == 0)
    {
        unit.ResetTurn(services);
    }

    const i32 taken = battle.turnTaken[static_cast<std::size_t>(self)];
    battle.unk5772BC = 1;
    battle.unk5772C0 = 1;
    battle.unk5376B8[at] = 1;
    battle.turnUnit = self;

    if (taken != kOrderedTurn)
    {
        battle.InsertSlotRecord(1, self, -1);
        return;
    }

    if (battle.slotRecords[0].unit != -1)
    {
        battle.InsertSlotRecord(1, self, 0);
        return;
    }

    PendingOrder& order = battle.pendingOrders[at];
    battle.actingUnit = self;
    battle.unk575F20 = 1;
    battle.unk577270 = 1;
    battle.wholeSide = order.unk04;
    battle.unk575F24 = order.unk00;
    battle.hitsAtOnce = order.unk08;
    battle.slideBackPicture = static_cast<i16>(order.unk0C);
    battle.sweepTotal = order.targetCount;
    order = PendingOrder{};
}

// The order of a role whose target is set in RoleFrame (inline in the original). A sweep order (unk04) gathers its
// targets (RoleGatherSweep) and then takes its command (RoleTakeOrder). A single order of a ready role takes its
// command at its aim: a party role at a role (unk00: aimTarget, 1-based) or at the menu's enemy (aimTarget); an AI
// role leaves the menu at its random enemy (aiTarget).
void Game::RoleOrderFrame(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;
    const auto at = static_cast<std::size_t>(role);
    PendingOrder& order = battle.pendingOrders[at];
    const bool automatic = unit.AutoControl() != 0 || unit.GuestControl() != 0 || unit.Ghost() != 0;
    const i32 command = battle.turnTaken[at + static_cast<std::size_t>(BattleState::FirstRoleUnit)];

    auto taken = [&battle]
    {
        battle.unk57725C = 0;
        battle.unk577258 = 0;
        battle.turnUnit = 0;
    };

    if (order.unk04 == 1)
    {
        this->RoleGatherSweep(unit, order);
        this->RoleTakeOrder(unit, role);
        return;
    }

    if (unit.GetValueAB4() != 0)
    {
        return;
    }

    if (order.unk00 != 0)
    {
        if (!automatic)
        {
            this->battleRoles[static_cast<std::size_t>(battle.aimTarget - 1)].SetAC0(0);
            unit.SetAD4(0);

            if (unit.SetCommand(command) == 1)
            {
                unit.Retarget(static_cast<i16>(battle.aimTarget - 1));
                unit.PrepareCommand(this->battleServices, this->database);
                unit.SetTarget(0);
                taken();
                battle.menuCommand = 1;
                battle.pageListRow = 1;
            }

            return;
        }
    }
    else
    {
        unit.SetAD4(0);

        if (!automatic)
        {
            if (unit.SetCommand(command) == 1)
            {
                this->battleEnemies[static_cast<std::size_t>(battle.aimTarget - 1)].SetAC0(0);
                unit.Retarget(static_cast<i16>(battle.aimTarget - 1));
                unit.SetTarget(0);
                unit.PrepareCommand(this->battleServices, this->database);
                taken();
                battle.pageListRow = 1;
                battle.specialRow = 1;
            }

            return;
        }
    }

    // An AI role clears the enemy of its aim even when it aims at a role, as in the original.
    this->battleEnemies[static_cast<std::size_t>(battle.aiTarget - 1)].SetAC0(0);
    unit.Retarget(static_cast<i16>(battle.aiTarget - 1));
    unit.SetTarget(0);
    taken();
    battle.aiTarget = 1;
}

// The targets of a sweep order of RoleOrderFrame (inline in the original): on the roles (unk5772D0) every role in
// battle not an escort, a guest, the ghost or StatusBit13 while the acting role is not ready, and with every role
// standing gathered the last role standing is aimed at; on the enemies every enemy standing without a pending removal
// (the count in targetCount), and with every one gathered the first one standing (unk57727C).
void Game::RoleGatherSweep(BattleRole& unit, PendingOrder& order)
{
    BattleState& battle = this->battleState;
    i32 aim = -1;

    if (battle.unk5772D0 != 0)
    {
        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            BattleRole& other = this->battleRoles[index];

            if (other.AutoControl() != 1 && other.GuestControl() != 1 && other.Ghost() != 1 &&
                other.StatusBit13() != 1 && unit.GetValueAB4() == 0)
            {
                other.SetAC0(0);
                ++order.targetCount;
            }
        }

        const auto standing = static_cast<i32>(battle.roleCount) - battle.unk5770D4 - battle.defeatedRoles -
                              battle.escortCount - battle.guestCount;

        if (order.targetCount < standing)
        {
            return;
        }

        for (i32 index = static_cast<i32>(battle.roleCount) - 1; index >= 0; --index)
        {
            if (!this->battleRoles[static_cast<std::size_t>(index)].IsDefeated())
            {
                aim = index;
                break;
            }
        }
    }
    else
    {
        i32 skipped = 0;

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            BattleEnemy& enemy = this->battleEnemies[index];

            if (enemy.IsDefeated() || battle.unk513664[index] != -1)
            {
                ++skipped;
                continue;
            }

            enemy.SetAC0(0);
            ++order.targetCount;
        }

        if (order.targetCount < static_cast<i32>(battle.enemyCount) - skipped)
        {
            return;
        }

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            if (!this->battleEnemies[index].IsDefeated())
            {
                aim = static_cast<i32>(index);
                break;
            }
        }
    }

    if (aim < 0)
    {
        return;
    }

    battle.unk57727C = 1;
    unit.SetAA8();
    unit.Retarget(static_cast<i16>(aim));
}

// A sweep order takes its command in RoleOrderFrame (inline in the original): an AI role just leaves the menu; a party
// role takes the command of its menu (command 4 also takes its command 2F18), prepares it and the menu resets.
void Game::RoleTakeOrder(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;

    if (unit.GetAA8() != 1)
    {
        return;
    }

    if (unit.AutoControl() != 0 || unit.GuestControl() != 0 || unit.Ghost() != 0)
    {
        battle.unk57725C = 0;
        unit.SetTarget(0);
        battle.unk577258 = 0;
        battle.unk577284 = 0;
        battle.unk57727C = 0;
        return;
    }

    const i32 command = battle.turnTaken[static_cast<std::size_t>(role + BattleState::FirstRoleUnit)];

    if (unit.SetCommand(command) != 1)
    {
        return;
    }

    unit.SetAD4(0);
    battle.unk57725C = 0;
    unit.SetTarget(0);

    if (command == kMagicAttackCommand)
    {
        unit.TakeCommand2F18();
    }

    unit.PrepareCommand(this->battleServices, this->database);
    battle.unk577258 = 0;
    battle.unk577284 = 0;
    battle.unk57727C = 0;
    battle.turnUnit = 0;
    battle.menuCommand = 1;
    battle.pageListRow = 1;
    battle.aimTarget = 1;
    battle.specialRow = 1;
}

// The acting role of RoleFrame (inline in the original); nothing while the battle waits (hitsAllRoles) before it
// acts. Once its target stands (unkAAC clear) the role starts acting: a sweep needs an enemy standing; a single hit at
// a defeated enemy passes to the first enemy standing (unless a taking holds it, unk577330); the skill name shows and
// the cost is paid. While acting it runs RoleActionFrame; once that is over the action ends (RoleAfterAction).
void Game::RoleActFrame(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;

    if (battle.retargeted != 0)
    {
        battle.unk577260 = 1;
        battle.unk5772BC = 1;
        battle.darkenUnit = static_cast<i16>(battle.actingUnit);
        unit.MarkAC8();

        if (this->RoleActionFrame(role, static_cast<i16>(unit.GetWord2EAE())) == 1)
        {
            this->RoleAfterAction(unit, role);
        }

        return;
    }

    if (battle.hitsAllRoles != 0)
    {
        return;
    }

    BattleEnemy& aimed = this->battleEnemies[static_cast<std::size_t>(static_cast<i16>(unit.GetWord2EAE()))];

    if (aimed.GetValueAAC() != 0)
    {
        return;
    }

    battle.retargeted = 1;
    battle.darkenUnit = static_cast<i16>(battle.actingUnit);

    if (unit.GetAA8() != 0)
    {
        if (battle.unk575F24 == 0)
        {
            bool standing = false;

            for (u32 index = 0; index < battle.enemyCount; ++index)
            {
                if (!this->battleEnemies[index].IsDefeated())
                {
                    standing = true;
                }
            }

            if (!standing)
            {
                battle.retargeted = 0;
                return;
            }
        }
    }
    else if (battle.unk575F24 == 0 && battle.pendingAction == 0 && aimed.IsDefeated() && battle.unk577330 == 0)
    {
        u32 first = 0;

        while (first < battle.enemyCount && this->battleEnemies[first].IsDefeated())
        {
            ++first;
        }

        if (first >= battle.enemyCount)
        {
            battle.retargeted = 0;
            return;
        }

        unit.Retarget(static_cast<i16>(first));
    }

    if (battle.retargeted == 1)
    {
        if (unit.skillName[0] != '\0')
        {
            this->battleMessages.Add(kNoticeX, 0, kSkillNameFrames, unit.skillName, BattleMessage::Rise);
            battle.listName = {};
        }

        unit.FinishCommand();
    }
}

// The end of a role's action in RoleActFrame (inline in the original): with the ghost out since role 0 retreated and
// role 0 standing again the ghost comes back (status 0x200); the roles aiming at the target turn away (RetargetRoles),
// a defeated enemy target with no enemy left ends the roles' effects, the enemies face again after a facing change;
// the role's turn and the acting state reset, the lists and marks of the menu too; a fight talk countdown steps, the
// poison spreads and a role low on spirit or stamina enrages the enemies (0x19, 0x1A).
void Game::RoleAfterAction(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if (battle.unk577370 == 1 && !this->battleRoles[0].IsDefeated())
    {
        BattleRole& ghost = this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)];
        battle.unk577370 = 0;
        ghost.ClearStatus(kGhostOutStatus1);
        ghost.ClearStatus(kGhostOutStatus2);
        ghost.ClearStatus(kGhostOutStatus3);
        ghost.SetStatus(kGhostBackStatus);
    }

    const auto target = static_cast<i16>(unit.GetWord2EAE());
    battle.unk5771EB = 0;
    battle.unk577260 = 0;
    battle.shake = 0;
    battle.unk53742C = 0;

    if (battle.unk5772C8 == 0)
    {
        battle.unk577270 = 0;
        battle.unk5772BC = 0;

        if (battle.unk577290 == 1)
        {
            battle.undarkening = 1;
        }
    }

    battle.unk5772C0 = 0;
    battle.unk5772D0 = 0;
    battle.retargeted = 0;

    if (target != -1)
    {
        this->RetargetRoles(role, target);

        if (this->battleEnemies[static_cast<std::size_t>(target)].IsDefeated() &&
            static_cast<i32>(battle.enemyCount) + battle.unk5771EA - battle.enemiesGone == 0)
        {
            for (u32 index = 0; index < battle.roleCount; ++index)
            {
                this->battleRoles[index].ExpireEffects(services, 1);
            }
        }

        if ((battle.panelFlags & kFacingPanel) != 0)
        {
            for (u32 index = 0; index < battle.enemyCount; ++index)
            {
                BattleEnemy& other = this->battleEnemies[index];

                if (!other.IsDefeated())
                {
                    other.SetFacingIfAble(0);
                }
            }

            battle.panelFlags &= ~kFacingPanel;
        }
    }

    unit.ResetTurn(services);

    if (battle.ghostRescued == 1)
    {
        battle.ghostRescued = 0;
    }

    battle.ClearActing();
    const auto at = static_cast<std::size_t>(role);
    battle.nameFlashes[at] = 0;
    battle.unk50D88C[at] = 0;
    battle.unk5376B8[at] = 0;
    battle.roleListScrolls.fill(0);
    battle.roleListRows.fill(1);
    battle.unk57723C = 0;
    battle.unk57723E = 0;
    battle.actingUnit = -1;
    battle.proficiencyBonus = 0;
    battle.spellStage = 0;
    battle.orderSorted = 0;
    battle.enemyFaded = 1;

    if ((this->fightTalk.waitCount & kTalkCountdown) != 0)
    {
        --this->fightTalk.waitCount;
    }

    this->SpreadPoison(0);

    if (unit.LowSpirit())
    {
        EnrageEnemies(services, kLowSpiritCause);
    }

    if (unit.LowStamina())
    {
        EnrageEnemies(services, kLowStaminaCause);
    }

    unit.ClearWord2EAE();
}

// The retreat of RoleFrame (inline in the original). While asked (kRetreatAsked) each role steps back; once all have
// (sweptUnits), the first role's level is rolled against the highest level of the enemies standing (200 while the
// removal of the enemy of the role's index is pending, kept; none standing: level 0 and the retreat is dropped): won,
// the retreat runs (kRetreating); lost, the roles come back with 撤退失敗!!, the role of the turn resets and the
// enemies enrage (0x16). While retreating role 0 holds the talisman ghost still and guarding, each role leaves once
// (its bit), and with every role gone 撤退成功!! ends the battle (state 0x68).
void Game::RoleRetreatFrame(BattleRole& unit, int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    const auto standing = [&battle]
    {
        return static_cast<i32>(battle.roleCount) - battle.unk5770D4 - battle.defeatedRoles - battle.escortCount -
               battle.guestCount;
    };

    if ((battle.unk577204 & kRetreatAsked) != 0)
    {
        battle.unk5772BC = 1;
        battle.unk5772C0 = 1;

        if (unit.StepBack(services, 0) == 1 && ++battle.sweptUnits >= static_cast<u32>(standing()))
        {
            battle.levelUpFrames = 0;

            for (u32 index = 0; index < battle.enemyCount; ++index)
            {
                BattleEnemy& enemy = this->battleEnemies[index];

                if (enemy.IsDefeated())
                {
                    continue;
                }

                battle.lastDefeatedObject = enemy.Data();

                // Without an object the original reads through a null pointer: taken as level 0.
                if (battle.lastDefeatedObject != nullptr &&
                    battle.lastDefeatedObject->data.level > battle.levelUpFrames)
                {
                    battle.levelUpFrames = battle.lastDefeatedObject->data.level;
                }

                if (battle.unk513664[static_cast<std::size_t>(role)] != -1)
                {
                    battle.levelUpFrames = kRetreatLevel;
                }
            }

            const u16 level = battle.levelUpFrames;

            if (level == 0)
            {
                battle.unk577204 = 0;
                battle.sweptUnits = 1;
                battle.levelUpFrames = 0;
                battle.unk5772BC = 0;
                battle.unk5772C0 = 0;
            }

            if (this->battleRoles[0].BeatsLevel(services, level) == 1)
            {
                unit.SetValueAAC(1);
                battle.unk577258 = 0;
                battle.currentUnit = 0;
                battle.levelUpFrames = 0;
                battle.unk577204 = kRetreating;
                battle.sweptUnits = static_cast<u16>(standing());
            }
            else
            {
                for (u32 index = 0; index < battle.roleCount; ++index)
                {
                    this->battleRoles[index].SetValueAAC(0);
                }

                if (this->battleMessages.messages == nullptr)
                {
                    this->battleMessages.Add(kNoticeX, kNoticeY, kRetreatFrames, kRetreatFailed, kNoticeFlags);
                }

                this->battleRoles[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].ResetTurn(
                    services
                );
                this->audio.PlaySound(kRetreatFailedSound, this->config.soundVolume);
                battle.unk577204 = 0;
                battle.sweptUnits = 1;
                battle.levelUpFrames = 0;
                battle.unk5772BC = 0;
                battle.unk5772C0 = 0;
                EnrageEnemies(services, kRetreatFailedCause);
            }
        }
    }

    if ((battle.unk577204 & kRetreating) == 0)
    {
        return;
    }

    battle.unk5772BC = 1;
    battle.unk5772C0 = 1;
    battle.unk577258 = 0;
    battle.currentUnit = 0;

    if (role == 0)
    {
        BattleRole& ghost = this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)];
        ghost.SetAF8(1);
        ghost.SetGuarding(1);
    }

    const auto bit = static_cast<u16>(1 << role);

    if ((battle.unk577204 & kRetreatRoles & bit) != 0 || unit.StepBack(services, 1) != 1)
    {
        return;
    }

    battle.enemyFaded = 1;
    battle.unk577204 = static_cast<u16>(battle.unk577204 | bit);

    if (++battle.defeatedRoles >=
        static_cast<u32>(battle.roleCount - battle.unk5770D4 - battle.escortCount - battle.guestCount))
    {
        this->battleMessages.Add(kNoticeX, kNoticeY, kRetreatFrames, kRetreated, kNoticeFlags);
        battle.roleOrder.fill(0);
        battle.slotRecords.fill(BattleSlotRecord{0, 0, 0, 0, 0, 0, 0, 0});
        battle.result = kRetreatedState;
        battle.unk577204 = 0;
        battle.actingUnit = -1;
    }
}
}  // namespace swd3e
