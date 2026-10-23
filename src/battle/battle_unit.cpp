#include "battle/battle_unit.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include "audio/audio_manager.h"
#include "battle/battle_enemy.h"
#include "battle/battle_hud.h"
#include "battle/battle_message.h"
#include "battle/battle_role.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/battle_unit_constants.h"
#include "battle/smooth/smooth_unit.h"
#include "config/config.h"
#include "core/format.h"
#include "core/words.h"
#include "crt/msvc.h"
#include "draw/paint_record.h"
#include "draw/painter.h"
#include "game/scene.h"
#include "org/database.h"
#include "org/fame.h"
#include "party/party.h"
#include "pic/picture.h"
#include "pic/rle.h"
#include "platform/display.h"
#include "random/random.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e
{

namespace
{

constexpr double kGaugePixels = 62.0;
constexpr int kSlotActsPerTurn = 8;

constexpr int kGaugeBase = 0x12C;  // a random gauge: kGaugeBase + kGaugeSpread / (1..kGaugeDivisors)
constexpr int kGaugeSpread = 0x96;
constexpr u32 kGaugeDivisors = 9;
constexpr std::size_t kClearedNameBytes = 0x10;  // ResetTurn clears only the start of the skill name
constexpr int kRoleGaugeBase = 0x32;             // a party role starts a turn at kRoleGaugeBase + 0..kRoleGaugeSpread-1
constexpr u32 kRoleGaugeSpread = 0x8C;

constexpr i16 kSpeedMask = 0x7FFF;  // the speed of a command with bit 15 set
constexpr i32 kUnkBlock30 = 0x30;
constexpr u16 kUnk2EB2Accessory = 0x27;  // the accessory kind whose proficiency bounds unk2EB2 (swd3e)
constexpr u16 kUnk2EB2Share = 10;        // unk2EB2 is held below a tenth of it
constexpr int kTens = 10;
constexpr i8 kNumberSpent = -1;  // unk2FD1: no use left
constexpr i8 kNumberTaken = -2;  // unk2FD1: the hit number was taken back
constexpr u16 kLastedTurns = 3;  // a lasting poison has lasted once past them
constexpr int kRippleGrid = 0xC8;
constexpr int kRippleSplashX = 0x64;
constexpr int kRippleSplashY = 0x32;
constexpr int kRippleRadius = 0x20;
constexpr int kRippleDepth = 0xC8;
constexpr int kRippleLeft = 0x50;  // the ripple is moved this far left of and kRippleUp above the unit
constexpr int kRippleUp = 0x78;
constexpr std::size_t kRippleSlot = 10;
constexpr i32 kRippleAct = 0x17E7;
constexpr int kEnrageEvent0 = 0;  // CountEnrageEvent
constexpr int kEnrageEvent1 = 1;
constexpr int kEnrageEvent2 = 2;
constexpr int kEnrageEvent3 = 3;
constexpr u16 kCounter2FD5Condition = 4;  // the enrage conditions the counters serve
constexpr u16 kSpent2FD5Condition = 5;
constexpr u16 kCounter2FD6Condition = 6;
constexpr u16 kSpent2FD6Condition = 7;
constexpr i8 kCounterSpent = -1;
constexpr u32 kSupportBuffs = SkillScript::Invisible | SkillScript::AttackUp | SkillScript::DefenseUp |
                              SkillScript::SpeedUp | SkillScript::Barrier;
constexpr u8 kSupportKind = 4;                 // ObjectData::kind bit 2
constexpr u32 kCommandStatusBit = 0x10000000;  // command +0x08 bit 28
constexpr u16 kAccessoryValueMask = 0x3FFF;
constexpr u16 kUnkAccessory28 = 0x28;  // the accessory kind UnkEquipment100 takes the value of (swd3e)
constexpr u32 kHealRoll = 0xA;         // ChooseHeal: a roll of 0..4 picks the first way, 5..9 the second
constexpr u8 kDefeatPoses = 10;        // a defeated unit takes a pose of 1..kDefeatPoses
constexpr u8 kFallbackDefeatPose = 6;  // or this one when none was taken
constexpr u16 kTurnFlashFall = 8;      // the tint pulse falls from here after ResetTurn
constexpr int kResetClearsGuard = 8;   // ResetTurn mode bit

constexpr u32 kEvadeRoll = 10;       // evade chances are in tenths
constexpr u16 kInvisibleEvades = 9;  // an invisible unit evades 9 times in kEvadeRoll
constexpr u32 kLikelyEvadeOdds = 3;
constexpr u8 kLowEvadeSkill = 6;  // up to this evade skill a hit also needs a roll of kLowEvadeGate or more
constexpr u16 kLowEvadeGate = 8;
constexpr int kTenths = 10;
constexpr int kThousands = 1000;  // the places of the four damage digits
constexpr int kHundreds = 100;
constexpr std::size_t kThousandsDigit = 0;
constexpr std::size_t kHundredsDigit = 1;
constexpr std::size_t kTensDigit = 2;
constexpr std::size_t kOnesDigit = 3;
constexpr int kAgilityShift = 2;  // the gauge step is agility / 4, agility / 8 when slowed
constexpr int kSlowedAgilityShift = 3;
constexpr int kBoostDivisor = 4;         // a boost adds a quarter of the step plus one
constexpr u16 kHealBelowShare = 3;       // an enemy heals below a third of its hit points
constexpr u8 kFirstHeal = 1;             // healChoice: ObjectData::healCountA
constexpr u8 kSecondHeal = 2;            // ObjectData::healCountB
constexpr u8 kSureEvadeStreak = 0x18;    // evades in a row from which the next hit lands
constexpr u8 kLikelyEvadeStreak = 0x14;  // from which it lands one time in three
constexpr int kPercent = 100;
constexpr int kSpeedChangePercent = 0x1E;  // the gauge step a speed buff adds or takes
constexpr int kInvisibleSlowPercent = 0xA;
constexpr int kChanceScale = 0x46;  // RollChance: kChanceScale percent of the chance plus kChanceBase
constexpr int kChanceBase = 0xA;
}  // namespace

// swd3e.exe 0x0048B040
BattleUnit::BattleUnit() = default;

// swd3e.exe 0x0048B100: frees the description of the unit's own object (+0xB0)
BattleUnit::~BattleUnit() = default;

// swd3e.exe 0x0048B130
int BattleUnit::SetTarget(int value)
{
    this->target = value;
    return value;
}

// swd3e.exe 0x0048B140: the action gauge scaled to 62 pixels of the full gauge
int BattleUnit::GaugeLength(const BattleServices& services) const
{
    return static_cast<i32>(
        crt::Ftol(static_cast<double>(this->gauge) / static_cast<double>(services.state.gaugeFull) * kGaugePixels)
    );
}

// swd3e.exe 0x0048B1C0
i16 BattleUnit::ResetGauge(const BattleServices& services)
{
    this->gauge = static_cast<u16>(services.state.gaugeFull);
    return static_cast<i16>(services.state.gaugeFull);
}

// swd3e.exe 0x0048B1D0
int BattleUnit::RandomizeGauge(BattleServices& services)
{
    const int value = kGaugeSpread / static_cast<int>(services.random.Below(kGaugeDivisors) + 1) + kGaugeBase;
    this->gauge = static_cast<u16>(value);
    return value;
}

// swd3e.exe 0x0048B230: the saved placement while drawAtSaved is set
void BattleUnit::GetDrawPosition(u16& x, u16& y) const
{
    const UnitPlacement& at = this->drawAtSaved != 0 ? this->savedPlacement : this->placement;
    x = static_cast<u16>(at.x);
    y = static_cast<u16>(at.y);
}

// swd32.exe 0x00478400 (unmatched in swd3e): the anchor of the main action (of the slot act 8 with flag 2 of +0x2A87);
// a mirrored unit
// measures a non-zero anchor x from the right edge of its shadow picture
void BattleUnit::GetOffset(u16& x, u16& y) const
{
    x = this->mainAct.anchorX;
    y = this->mainAct.anchorY;

    if ((this->commandFlags & BattleUnit::UnkCommandFlag200) != 0)
    {
        x = this->slotActs[BattleUnit::UnkSlot8].anchorX;
        y = this->slotActs[BattleUnit::UnkSlot8].anchorY;
    }

    if (this->facing == 1)
    {
        const u16 offset = this->mainAct.anchorX;

        if (offset != 0 && this->shadowPicture != nullptr)
        {
            x = static_cast<u16>(this->shadowPicture->width - offset);
        }
    }
}

// swd32.exe 0x00478470 (unmatched in swd3e)
i16 BattleUnit::GetOrigin(u16& x, u16& y) const
{
    x = static_cast<u16>(this->placement.x - this->originX);
    const auto value = static_cast<u16>(this->placement.y - static_cast<u16>(this->mainAct.y));
    y = value;
    return static_cast<i16>(value);
}

// swd3e.exe 0x0048B3A0: the rectangle of the shadow picture (action 0x24 of the unit's act, or the main action's,
// 0x27 while defeated) under the unit
void BattleUnit::GetShadowRect(BattleServices& services, UnitRect& rect)
{
    if (this->defeated == 1 && this->kind == 0)
    {
        return;
    }

    if (this->actId == 0)
    {
        return;
    }

    ActState state;
    state.action = kShadowAction;
    state.id = this->actId;

    if (this->mainAct.action != 0)
    {
        state.action = this->mainAct.action;
    }

    if (this->defeated == 1)
    {
        state.action = kDefeatedShadowAction;
    }

    services.acts.Run(state);
    this->picture = services.archive.GetPicture(state.frame, state.picture);

    if (this->picture == nullptr)
    {
        return;
    }

    if (this->facing == 1)
    {
        state.x = this->picture->width - state.x;
    }

    rect.x = this->placement.x - state.x;
    rect.y = this->placement.y - state.y;
    rect.width = this->picture->width;
    rect.height = this->picture->height;
}

// swd3e.exe 0x0048B4A0
i16 BattleUnit::Move(i16 dx, i16 dy)
{
    this->placement.x = static_cast<i16>(this->placement.x + dx);
    this->placement.y = static_cast<i16>(this->placement.y + dy);
    return dx;
}

// swd3e.exe 0x0048B4C0: also saves the placement
i16 BattleUnit::SetPosition(i16 x, i16 y)
{
    this->placement.x = x;
    this->placement.y = y;
    this->savedPlacement = this->placement;
    return x;
}

// swd3e.exe 0x0048B500
i16 BattleUnit::GetPosition(u16& x, u16& y) const
{
    x = static_cast<u16>(this->placement.x);
    const auto value = static_cast<u16>(this->placement.y);
    y = value;
    return static_cast<i16>(value);
}

// swd3e.exe 0x0048B520: runs a copy of the main action in the step action and fetches its picture
const Picture* BattleUnit::StepIdleAction(BattleServices& services)
{
    this->stepAct = this->mainAct;

    if (services.acts.Run(this->stepAct) == 0)
    {
        return nullptr;
    }

    this->picture = services.archive.GetPicture(this->stepAct.frame, this->stepAct.picture);
    return this->picture;
}

// swd3e.exe 0x0048B570: the battle attributes (second) or the party record of a party role
const void* BattleUnit::SelectEntry(int second) const
{
    if (second != 0)
    {
        return this->battleRecord;
    }

    return this->roleRecord;
}

// swd3e.exe 0x0048B590
int BattleUnit::GetValueAAC() const
{
    return this->unk2AAC;
}

// swd3e.exe 0x0048B5A0
int BattleUnit::GetValueAB4() const
{
    return this->unk2AB4;
}

// swd3e.exe 0x0048B5B0
u16 BattleUnit::GetCommandA() const
{
    return this->commandA;
}

// swd3e.exe 0x0048B5C0
u16 BattleUnit::GetCommandB() const
{
    return this->commandB;
}

// swd3e.exe 0x0048B5D0
u16 BattleUnit::GetWord2A74() const
{
    return this->unk2A74;
}

// swd3e.exe 0x0048B5E0
u16 BattleUnit::GetWord29A2() const
{
    return this->unk29A2;
}

// swd3e.exe 0x0048B5F0
int BattleUnit::ClearIdleAction()
{
    this->mainAct = ActState{};
    return 0;
}

// swd3e.exe 0x0048B610: commands 2, 3, 6, 13, 14 and 15 go to commandB (6 and 14 / 15 also set flags of +0x2A87), any
// other to commandA
int BattleUnit::SetCommand(int command)
{
    if (command == BattleUnit::UnkCommand14 || command == BattleUnit::UnkCommand15)
    {
        this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag4000);
    }
    else if (command == BattleUnit::UnkCommand6)
    {
        this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag800);
    }
    else if (
        command != BattleUnit::UnkCommand2 && command != BattleUnit::UnkCommand13 && command != BattleUnit::UnkCommand3
    )
    {
        this->commandA = static_cast<u16>(command);
        return 1;
    }

    this->commandB = static_cast<u16>(command);
    this->commandA = 0;
    return 1;
}

// swd3e.exe 0x0048B680
void BattleUnit::ClearFlag26B8()
{
    this->unk26B8 &= ~kFlag26B8;
}

// swd3e.exe 0x0048B690: a hit staggers the unit unless status bit 25 or 2 is set, an effect with bit 1 runs or flag
// 26 of +0x2BC0 is set: under status bit 17 outside a cast spell only flag 27 is set; otherwise flag 13 marks pending
// damage, the stagger (hurtStep) restarts and flag 31 is set
void BattleUnit::RaiseFlag26B8(const BattleState& battle)
{
    if ((this->status & BattleUnit::ResistAll) != 0 || this->HasEffectBits(kStaggerBlockingEffect) == 1)
    {
        return;
    }

    if ((this->status & BattleUnit::UnkStatus4) != 0 || (this->unk26B8 & kFlag26B8Blocked) != 0)
    {
        return;
    }

    if ((this->status & BattleUnit::DamageTransfer) != 0 && (battle.spellStage & BattleState::SpellCast) == 0)
    {
        this->unk26B8 |= kFlag26B8Spell;
        return;
    }

    if (this->pendingDamage != 0)
    {
        this->unk26B8 |= kFlag26B8Damage;
    }

    this->hurtStep = 0;
    this->unk26B8 |= kFlag26B8;
}

// swd3e.exe 0x0048B720
int BattleUnit::Flag26B8() const
{
    return (this->unk26B8 & kFlag26B8) != 0 ? 1 : 0;
}

// swd3e.exe 0x0048B730: stores at the current count without advancing it
int BattleUnit::PushTarget(u16 value)
{
    const u16 count = this->targetCount;
    this->pushedTargets[count] = value;
    return count;
}

// swd3e.exe 0x0048B750: a defeated role has its hit points set to 0
int BattleUnit::StartDefeat(int defeat)
{
    this->defeated = defeat;
    this->dying = 1;

    if (this->defeatPose == 0)
    {
        this->defeatPose = kFallbackDefeatPose;
    }

    if (this->kind == BattleUnit::PartyRole && this->roleRecord != nullptr)
    {
        this->roleRecord->hitPoints = 0;
    }

    return 1;
}

// swd32.exe 0x00478830 (unmatched in swd3e)
int BattleUnit::ToggleAF8()
{
    const int value = this->unk2AF8 == 0 ? 1 : 0;
    this->unk2AF8 = value;
    return value;
}

// swd32.exe 0x00478850 (unmatched in swd3e): clears the state of a turn; roles get a random gauge of 50..189. The slot
// actions cleared
// are the first eight.
int BattleUnit::ResetTurn(BattleServices& services)
{
    this->gauge = 0;
    this->unk2AAC = 0;
    const u16 mode = this->command.flags;
    this->unk2AB0 = 0;
    this->unk2AB4 = 0;
    this->unk2AA8 = 0;
    this->unk2AE0 = 0;
    this->evadeRolled = 0;
    this->unk2A8C = 0;
    this->commandFlags = 0;
    this->unk2AC8 = 0;
    this->unk2AD4 = 0;
    this->unk2ACC = 0;
    this->unk2B0C = 0;
    this->unk2AF4 = 0;
    this->commandA = 0;
    this->commandB = 0;
    this->unk2A72 = 0;
    this->unk2A7A = 0;
    this->unk29A4 = 0;
    this->fadeLevel = BattleUnit::Opaque;
    this->brightLevel = 1;
    this->slotTint = 0;
    this->flashPhase = BattleUnit::FlashStart;
    this->flashFall = kTurnFlashFall;
    this->unk2674 = 0;
    this->action = 0;
    this->readyAction = 0;
    this->homePoseX = 0;
    this->magicAction = 0;
    this->restoreHitPoints = 0;
    this->restoreSpirit = 0;
    this->restoreStamina = 0;
    this->mainAct.anchorX = 0;
    this->mainAct.anchorY = 0;
    this->unk26CC = 0;

    if ((mode & kResetClearsGuard) != 0)
    {
        this->guarding = 0;
    }

    if ((this->command.actionFlags & UnitCommand::DelaysFadeIn) != 0)
    {
        this->guarding = 0;
        this->brightLevel = BattleUnit::DelayedFadeIn;
        this->marks = static_cast<u16>((this->marks & ~BattleUnit::UnkMark8) | BattleUnit::UnkMark200);
    }

    if (this->autoControl == 1)
    {
        this->guarding = 1;
    }

    if (this->guestControl == 1)
    {
        this->placement = this->savedPlacement;
    }

    this->standAct = ActState{};
    this->castAct = ActState{};
    this->recoilAct = ActState{};
    this->attackAct = ActState{};

    for (int i = 0; i < kSlotActsPerTurn; ++i)
    {
        this->slotActs[static_cast<std::size_t>(i)] = ActState{};
    }

    std::fill_n(this->skillName.begin(), kClearedNameBytes, '\0');
    this->command = UnitCommand{};
    this->targets = {BattleUnit::NoTarget, BattleUnit::NoTarget, BattleUnit::NoTarget, BattleUnit::NoTarget};
    int result = this->kind;

    if (result == BattleUnit::PartyRole)
    {
        result = static_cast<int>(services.random.Below(kRoleGaugeSpread)) + kRoleGaugeBase;
        this->gauge = static_cast<u16>(result);
    }

    return result;
}

// swd32.exe 0x00478A70 (unmatched in swd3e)
i16 BattleUnit::Retarget(i16 value)
{
    this->unk2AB4 = 1;
    this->unk29A2 = static_cast<u16>(value);
    this->gauge = 0;
    return value;
}

// swd32.exe 0x00478AA0 (unmatched in swd3e)
void BattleUnit::IncreaseCounter2A76()
{
    ++this->unk2A76;
}

// swd32.exe 0x00478AB0 (unmatched in swd3e)
u16 BattleUnit::Counter2A76() const
{
    return this->unk2A76;
}

// swd3e.exe 0x0048BA10
void BattleUnit::IncreaseHits()
{
    ++this->unk2A74;
    this->unk2AE0 = 1;
}

// swd32.exe 0x00478AE0 (unmatched in swd3e)
i16 BattleUnit::DecreaseHits()
{
    if (this->unk2A74 != 0)
    {
        --this->unk2A74;
    }

    auto result = static_cast<i16>(this->unk2A76);

    if (result != 0)
    {
        --result;
        this->unk2A76 = static_cast<u16>(result);
    }

    this->unk2AE0 = 0;
    return result;
}

// swd3e.exe 0x0048BA30
void BattleUnit::ClearWord29A2()
{
    this->unk29A2 = BattleUnit::NoUnk29A2;
}

// swd3e.exe 0x0048BA40
void BattleUnit::SetAA8()
{
    this->unk2AA8 = 1;
}

// swd32.exe 0x00478B40 (unmatched in swd3e)
int BattleUnit::GetAA8() const
{
    return this->unk2AA8;
}

// swd32.exe 0x00478B50 (unmatched in swd3e)
int BattleUnit::GetAE0() const
{
    return this->unk2AE0;
}

// swd32.exe 0x0047C660 (unmatched in swd3e)
int BattleUnit::SetAC0(int value)
{
    this->outline = value;
    return value;
}

// swd32.exe 0x0047C670 (unmatched in swd3e)
bool BattleUnit::IsAE4Clear() const
{
    return this->target == 0;
}

// swd32.exe 0x0047C680 (unmatched in swd3e)
u16 BattleUnit::GetWordDA6() const
{
    return this->command.unk16;
}

// swd3e.exe 0x00490390: unless flag 2 of +0x2A87 is set
void BattleUnit::MarkAC8()
{
    if ((this->commandFlags & BattleUnit::UnkCommandFlag200) == 0)
    {
        this->unk2AC8 = 1;
    }
}

// The draw flags of the recoil picture shared by the fades: a forced mirror (blend mode) or a facing unit flips bit 0
// and measures the origin from the right edge of the picture.
void BattleUnit::MirrorRecoil(u32 actFlags, bool forced)
{
    const u16 width = this->picture != nullptr ? this->picture->width : 0;

    if (forced)
    {
        if (this->facing == 1)
        {
            this->drawFlags =
                (actFlags & Painter::Mirror) != 0 ? actFlags & ~Painter::Mirror : actFlags | Painter::Mirror;
            this->originX = static_cast<u16>(width - static_cast<u16>(this->recoilAct.x));
        }

        this->drawFlags |= Painter::Additive;
    }

    if (this->facing == 1 && !forced)
    {
        this->drawFlags = (actFlags & 1) != 0 ? actFlags & ~1u : this->drawFlags | 1;
        this->originX = static_cast<u16>(width - static_cast<u16>(this->recoilAct.x));
    }
}

// swd3e.exe 0x004903B0: fades the unit out over fadeLevel (sound 0x2F on the first frame): translucent, or tinted
// darker in blend mode; 1 once it is gone
int BattleUnit::DrawFadeOut(BattleServices& services)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DrawFadeOutAdvance(services, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x004906F0: fades the unit in at the draw position of `anchor` (sound 0x30 on the first frame); 1 once it
// is fully visible
int BattleUnit::DrawFadeIn(BattleServices& services, const BattleUnit& anchor)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DrawFadeInAdvance(services, anchor, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x00490A30
int BattleUnit::SetHeld(int value)
{
    this->held = value;
    return value;
}

// swd3e.exe 0x00490A40
int BattleUnit::SetAD4(int value)
{
    this->unk2AD4 = value;
    return value;
}

// swd32.exe 0x0047CD60 (unmatched in swd3e): the evasion roll. Invisible units (status bit 31) evade 9 in 10; a given
// chance is out of 10;
// otherwise the role's evasion (+0x2A93) is rolled, never right after a success, forced after 24 failures in a row and
// one in three after 20 (a low evasion first passes an 80% no).
int BattleUnit::RollEvade(BattleServices& services, u16 chance)
{
    BattleState& state = services.state;

    auto success = [&]
    {
        state.evadeStreak = 0;
        state.evadeJustSucceeded = 1;
        this->marks = static_cast<u16>(this->marks | 1);
        return 1;
    };

    if (state.criticalAttack == 1)
    {
        return 0;
    }

    if (static_cast<i32>(this->status) < 0)
    {
        return static_cast<u16>(services.random.Below(kEvadeRoll) + 1) <= kInvisibleEvades ? success() : 0;
    }

    const u8 skill = this->evadeSkill;

    if (skill == 0)
    {
        return 0;
    }

    if (chance != 0)
    {
        return static_cast<u16>(services.random.Below(kEvadeRoll)) < chance ? success() : 0;
    }

    if (state.evadeJustSucceeded == 1)
    {
        state.evadeJustSucceeded = 0;
        return 0;
    }

    const u8 streak = state.evadeStreak;
    bool hit = false;

    if (streak >= kSureEvadeStreak)
    {
        hit = true;
    }
    else if (streak >= kLikelyEvadeStreak)
    {
        hit = static_cast<u16>(services.random.Below(kLikelyEvadeOdds)) == kLikelyEvadeOdds - 1;
    }
    else if (skill <= kLowEvadeSkill)
    {
        hit = static_cast<u16>(services.random.Below(kEvadeRoll)) >= kLowEvadeGate &&
              static_cast<u16>(services.random.Below(kEvadeRoll)) < this->evadeSkill;
    }
    else
    {
        hit = static_cast<u16>(services.random.Below(kEvadeRoll)) < this->evadeSkill;
    }

    if (hit)
    {
        return success();
    }

    ++state.evadeStreak;
    return 0;
}

// swd3e.exe 0x00490F90
int BattleUnit::Mirrored() const
{
    return static_cast<int>(this->drawFlags & 1);
}

// swd3e.exe 0x00490FA0
bool BattleUnit::IsDefeated() const
{
    return this->defeated == 1 || (this->marks & BattleUnit::DefeatedMark) != 0;
}

// swd3e.exe 0x00490FC0
int BattleUnit::StatusBit13() const
{
    return (this->marks & BattleUnit::DefeatedMark) != 0 ? 1 : 0;
}

// swd3e.exe 0x00490FD0: defeated, or its defeat under way
bool BattleUnit::IsDefeatedOrDying() const
{
    return this->defeated == 1 || this->dying == 1;
}

// swd3e.exe 0x00490FF0: +1 (capped at 0x22) or -1 on the target count
int BattleUnit::StepTargetCount(int increase)
{
    if (increase == 1)
    {
        const bool below = ++this->targetCount < BattleUnit::MaxTargets;
        this->unk2AD8 = 1;

        if (!below)
        {
            this->targetCount = BattleUnit::MaxTargets - 1;
        }
    }
    else
    {
        --this->targetCount;
        this->unk2AD8 = increase;
    }

    return increase;
}

// swd3e.exe 0x004910A0
int BattleUnit::PushTargetAlt(u16 value)
{
    const u16 count = this->targetCount;
    this->damageDelays[count] = value;
    return count;
}

// swd32.exe 0x0047D2E0 (unmatched in swd3e): value plus `tenths` tenths for -10..127, value * (tenths + 10) / 10 (an
// 8-bit sum) below
int ScaleByTenths(i8 tenths, i16& value)
{
    if (tenths >= -kTenths)
    {
        value = static_cast<i16>(value + tenths * value / kTenths);
        return 0;
    }

    value = static_cast<i16>(value * static_cast<i8>(tenths + kTenths) / kTenths);
    return 1;
}

// swd3e.exe 0x00491B90
int BattleUnit::ResetAfterGuard()
{
    this->unk2AEC = 1;
    this->gauge = 0;
    this->homePoseX = 0;
    return 0;
}

// swd3e.exe 0x00491BB0
int BattleUnit::GetGuard() const
{
    if ((this->marks & (BattleUnit::UnkMark2 | BattleUnit::Held)) != 0)
    {
        return 0;
    }

    return this->unk2AEC;
}

// swd3e.exe 0x00491BD0: mark 0x200 also darkens the unit (brightLevel -32)
int BattleUnit::SetStatus(int bits)
{
    if (static_cast<u16>(bits) == BattleUnit::StatsScaled)
    {
        this->brightLevel = BattleUnit::DelayedFadeIn;
    }

    this->marks = static_cast<u16>(this->marks | static_cast<u16>(bits));
    return bits;
}

// swd3e.exe 0x00491BF0: clearing mark 0x1000 also ends the guard
int BattleUnit::ClearStatus(int bits)
{
    this->marks = static_cast<u16>(this->marks & static_cast<u16>(~bits));

    if ((bits & BattleUnit::Poisoned) != 0)
    {
        this->guarding = 0;
    }

    return bits;
}

// swd3e.exe 0x00491C20
i16 BattleUnit::SetWord2A0A(i16 value)
{
    this->fadeStep = static_cast<u16>(value);
    return value;
}

// swd3e.exe 0x00491C30
int BattleUnit::SetGuarding(int value)
{
    this->guarding = value;
    return value;
}

// swd3e.exe 0x00491C40
int BattleUnit::ActFlag1() const
{
    return static_cast<int>((this->command.actionFlags >> 1) & 1);
}

// swd3e.exe 0x00491C60
int BattleUnit::ActFlag11() const
{
    return (this->command.actionFlags & UnitCommand::UnkAction800) != 0 ? 1 : 0;
}

// swd3e.exe 0x00491C70: the speed of the command when its bit 15 is set
i16 BattleUnit::ActSpeed() const
{
    const i16 speed = this->command.unk1E;
    return speed >= 0 ? i16{0} : static_cast<i16>(speed & kSpeedMask);
}

// swd3e.exe 0x00491C90
int BattleUnit::ActFlag0() const
{
    return static_cast<int>(this->command.actionFlags & UnitCommand::UnkAction1);
}

// swd32.exe 0x0047D8E0 (unmatched in swd3e)
int BattleUnit::ActFlag13() const
{
    return (this->command.actionFlags & UnitCommand::UnkAction2000) != 0 ? 1 : 0;
}

// swd32.exe 0x0047D8F0 (unmatched in swd3e)
int BattleUnit::ActFlag14() const
{
    return (this->command.actionFlags & UnitCommand::UnkAction4000) != 0 ? 1 : 0;
}

// swd32.exe 0x0047D900 (unmatched in swd3e): also restarts the main action
int BattleUnit::SetAction(i16 value)
{
    this->action = static_cast<u16>(value);
    this->mainAct = ActState{};
    return 0;
}

// swd32.exe 0x0047D930 (unmatched in swd3e)
bool BattleUnit::IsGuarding() const
{
    return this->guarding == 1 || (this->marks & BattleUnit::GuardingMark) != 0;
}

// swd3e.exe 0x00491D60: roles set the current and maximum hit points, spirit and stamina of their party record
// (positive values; -1 refills the current value); enemies take the hit points as their maximum and current value
void BattleUnit::SetVitals(i16 hitPoints, i16 spirit, i16 stamina)
{
    if (this->kind == BattleUnit::PartyRole)
    {
        RoleRecord* record = this->roleRecord;

        if (record == nullptr)
        {
            return;
        }

        if (hitPoints > 0)
        {
            record->maxHitPoints = hitPoints;
            record->hitPoints = hitPoints;
        }

        if (hitPoints == -1)
        {
            record->hitPoints = record->maxHitPoints;
        }

        if (spirit > 0)
        {
            record->maxSpirit = spirit;
            record->spirit = spirit;
        }

        if (spirit == -1)
        {
            record->spirit = record->maxSpirit;
        }

        if (stamina > 0)
        {
            record->maxStamina = stamina;
            record->stamina = stamina;
        }

        if (stamina == -1)
        {
            record->stamina = record->maxStamina;
        }

        return;
    }

    if (this->data != nullptr)
    {
        this->data->data.maxHitPoints = hitPoints;
        this->data->data.hitPoints = hitPoints;
    }
}

// swd3e.exe 0x00491E10: positive values replace the attack, defence and agility (a party role's battle copy, not
// checked for null in the original; an enemy's data)
i16 BattleUnit::SetAttributes(i16 attack, i16 defense, i16 agility)
{
    if (this->kind == BattleUnit::PartyRole)
    {
        RoleRecord* record = this->battleRecord;

        if (record != nullptr)
        {
            if (attack > 0)
            {
                record->attackBonus = static_cast<u16>(attack);
            }

            if (defense > 0)
            {
                record->defenseBonus = static_cast<u16>(defense);
            }

            if (agility > 0)
            {
                record->agility = agility;
            }
        }

        return agility;
    }

    if (this->data == nullptr)
    {
        return 0;
    }

    if (attack > 0)
    {
        this->data->data.attack = static_cast<u16>(attack);
    }

    if (defense > 0)
    {
        this->data->data.defense = static_cast<u16>(defense);
    }

    if (agility > 0)
    {
        this->data->data.agility = static_cast<u16>(agility);
    }

    return agility;
}

// swd3e.exe 0x00491E90: an enemy's level
void BattleUnit::SetLevel(i16 level)
{
    if (this->kind != BattleUnit::PartyRole && this->data != nullptr)
    {
        this->data->data.level = static_cast<u16>(level);
    }
}

// swd32.exe 0x0047DAB0 (unmatched in swd3e)
int BattleUnit::SetB00(int value)
{
    this->autoControl = value;
    return value;
}

// swd32.exe 0x0047DAC0 (unmatched in swd3e)
int BattleUnit::SetB04(int value)
{
    this->guestControl = value;
    return value;
}

namespace
{

constexpr int kAnnounceFlags = BattleMessage::BlackBar | BattleMessage::Centered;
constexpr u32 kAilmentRoll = 10;  // an ailment lands on a roll of 1..kAilmentRoll up to the ailment chance

// The damage of a hit (HitDamage).
constexpr std::size_t kResistances = 9;
constexpr u32 kAttackFromBonus = 9;  // unk26CC bits: the attack is the bonus of the battle record (enraged)
constexpr u32 kLevelShare = 3;       // the attack roll is 1..level / kLevelShare + kLevelRollBase
constexpr u32 kLevelRollBase = 3;
constexpr int kAttackBuffPercent = 30;
constexpr int kEnragedPercent = 60;  // an enraged attack: 60% plus proficiency percent of 40%
constexpr int kEnragedSharePercent = 40;
constexpr double kBladeScale = 2.5;  // swd32.exe 0x00499D38 (unmatched in swd3e)
constexpr int kSkillScaleShare = 3;
constexpr u32 kSkillRollDivisors = 5;  // a skill rolls 1..(level + unk14) / (5..9)
constexpr u16 kSkillRollBase = 5;
constexpr int kHalf = 2;
constexpr int kGuardShare = 3;  // a guard takes a third off
// The acts of a scripted battle with a special effect, and the poses they leave.
constexpr u16 kFallAct = 1;
constexpr u8 kFallPose = 7;
constexpr u16 kDrainAct = 2;
constexpr u8 kDrainPose = 2;
constexpr u8 kDrainPoseAfter = 8;
constexpr u16 kWeakAct = 8;
constexpr int kWeakPercent = 85;
constexpr u8 kWeakPose = 0xD;
constexpr u16 kStrongAct = 0x11;
constexpr int kStrongPercent = 150;
constexpr u32 kStrongAilments = 3;
constexpr u32 kThirdAilment = 2;
constexpr u8 kStrongTurns = 3;
constexpr u8 kStrongPose = 1;

// FlyAct: the steps per frame of a speeding flight double up to kFlightDoubling, then grow by kFlightGrowth.

// DrawAction.

// DrawTrail.

// RunQueuedActions and TrailLastQueuedAction.
constexpr i8 kQueuedTrailImages = 3;

// ReturnHome.
constexpr int kHomeSteps = 0x23;
constexpr int kHomeShare = 12;  // 0x00482AC0: imul by 0x2AAAAAAB, sar 1

// WalkToTarget.
constexpr int kWalkSteps = 0x20;
constexpr int kWalkShare = 16;

// TickEffects, CanUseAccessory, the drain effects and CatchRoll.
constexpr u16 kAccessory19 = 0x19;
constexpr u32 kAccessory19Bit = 0x10;
constexpr u16 kAccessory1C = 0x1C;
constexpr u16 kAccessory1D = 0x1D;
constexpr u32 kAccessory1DBit = 0x100;
constexpr u16 kAccessory20 = 0x20;
constexpr u16 kSpiritFanKind = 0x39;
constexpr int kDrainLeft = 0x32;
constexpr int kDrainDown = 0x28;
constexpr u16 kDrainSpeed = 0x28;
constexpr u16 kDrainLife = 0x14;
constexpr u16 kDrainDivisor = 0x3C;

constexpr u32 kUnusedCatchRoll = 12;
constexpr u32 kCatchRoll = 10;
constexpr int kFarAbove = 10;
constexpr int kNearAbove = 5;
constexpr u16 kFarCatch = 8;
constexpr u16 kNearCatch = 5;

// HurtFrame.

// IdleFrame.
constexpr u16 kShaking = 1;  // drawAtSaved: the unit shakes while nudge lasts
constexpr u32 kShakeChance = 2;
constexpr i16 kRiseStep = 10;
constexpr i16 kDropStep = 0x1E;

// ChooseCommand.
constexpr int kNoChoice = 0xFFFF;
constexpr int kItemChoice = 0;
constexpr int kSkillChoice = 1;
constexpr int kHealChoice = 2;
constexpr int kStatusChoice = 4;  // a status of bit 5 or 8: the attack
constexpr int kIdleChoice = 5;    // no idle turn yet: the attack
constexpr u8 kItemsChosen = 1;    // choicePhase
constexpr u8 kSkillsChosen = 2;
constexpr u32 kUnusedChoiceRoll = 2;
constexpr u32 kChoiceRoll = 10;
constexpr u32 kFirstItemRoll = 4;   // a roll up to it takes the first object
constexpr u32 kSecondItemRoll = 7;  // the second
constexpr u32 kFirstSkillRoll = 6;
constexpr u8 kItemWaitTurns = 7;
constexpr u8 kSkillWaitTurns = 5;
constexpr u32 kNoForcedStatus = 0x10000000;  // in UnitCommand::unk08: a forced unit (autoControl) does not choose it
constexpr i32 kFirstItemAction = 0x2C;       // the ready poses of an act kReadyAct by the object used
constexpr i32 kSecondItemAction = 0x2D;
constexpr i32 kThirdItemAction = 0x2E;
constexpr i32 kHealAction = 0x2F;
constexpr u16 kDelayedCommand = 0xC8;

// EnemyHitDamage.
constexpr u16 kSacrificeAilment = 0x8000;  // the ailment landed, the attacker falls (inferred)
constexpr int kSacrificeDamage = 0x270F;
constexpr u32 kEnemySpreadRoll = 5;  // the bonus of other commands rolls below (level + unk5C) / (5..9)
constexpr u32 kEnemySpreadBase = 5;
constexpr u32 kGuardCutRoll = 0x1E;  // a guard cuts 10..39%
constexpr u32 kGuardCutBase = 10;
constexpr u16 kFanStep = 5;
constexpr u16 kFanShare = 0x14;  // the spirit fan guards more by each 20% of proficiency
constexpr std::array<u16, 5> kFanGuards = {0x1080, 0x10C8, 0x10E9, 0x10EF, 0x12EF};

// RunAway.

// StepOrbCounters.
constexpr u16 kOrbStep = 0xC;  // the proficiency an element orb gains from a hit of its element

// DrawDamageNumbers.

// RollDrop.
constexpr u32 kDropRoll = 20;

// RefineTalisman.
constexpr u16 kTwoChoices = 0x32;
constexpr u16 kTwoChoicesEnd = 0x4F;
constexpr u16 kThreeChoices = 0x50;
constexpr u16 kThreeChoicesEnd = 0x64;
constexpr u32 kRefineObjects = 3;
constexpr u32 kRefineRoll = 100;
constexpr u16 kRefineChance = 0x3C;
constexpr u16 kSureRefine = 0x5A;
constexpr u32 kRefinedKind = 0x8000000;

// FadeInvisible and StepAccessoryCounters.

constexpr double kCounterPercent = 100.0;  // swd32.exe 0x00499D28 (unmatched in swd3e)

// StepGauge.
constexpr std::size_t kAgilityShare = 2;  // the gauge fills by a quarter of the agility a tick
constexpr int kBoostShare = 4;
constexpr int kInvisibleSlowdown = 4;

// FadeMainOut and Defense.

constexpr int kDefenseChangePercent = 30;
constexpr int kBarrierPercent = 70;

// DefeatFrame: the defeat poses (defeatPose) and their timing.
constexpr i32 kDefeatAction = 0x24;
constexpr i16 kDefeatedAtOnce = 0x3E8;
constexpr u8 kRisingPose = 1;

constexpr u8 kSplitRowsPose = 3;
constexpr u8 kSplitColumnsPose = 4;
constexpr u8 kFillPose = 5;
constexpr u8 kFadePose = 6;
constexpr u8 kQuartersPose = 7;
constexpr u8 kSweepPose = 8;
constexpr u8 kWavePose = 9;

constexpr u8 kSinkPose = 0xB;
constexpr u8 kZoomPose = 0xC;
constexpr u8 kStripsPose = 0xD;
constexpr u8 kHalvesPose = 0xE;
constexpr u8 kSlidePose = 0xF;
constexpr u8 kHopPose = 0x32;
constexpr u8 kProjectedPose = 0x33;
constexpr u8 kParticlePose = 0x64;

}  // namespace

// swd3e.exe 0x004910C0: while mark 1 is set, the four letters (frames 0..3 of digit font 0) pop in one after another
// (the later ones only advance once the first is past phase 0), then rise and fade out by 4 per frame
void BattleUnit::DrawGuardText(BattleServices& services)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).Guard(scene);
    scene.Show(services);
}

// swd3e.exe 0x00491A00: splits `amount` into the four digits of the current target slot (a set bit 15 negates it).
// While the barrier is up against a damage number, a hit only counts the barrier down and ends it with
// "抵抗所有攻擊解除".
void BattleUnit::SetDamageDigits(BattleServices& services, int amount)
{
    if ((this->marks & BattleUnit::UnkMark8000) != 0)
    {
        return;
    }

    const u16 slot = this->targetCount;

    if (this->pushedTargets[slot] == BattleUnit::DamageTarget && (this->status & BattleUnit::ResistAll) != 0 &&
        (amount & BattleUnit::Healing) == 0)
    {
        if (--this->barrierTurns != 0)
        {
            return;
        }

        this->Announce(services, BattleMessages::ResistAllEnd, kAnnounceFlags);
        this->status &= ~BattleUnit::ResistAll;
        return;
    }

    if ((amount & BattleUnit::Healing) != 0)
    {
        amount = -amount;
    }

    auto& digits = this->damageDigits[slot];
    digits[kThousandsDigit] = static_cast<i16>(amount / kThousands);
    amount -= kThousands * digits[kThousandsDigit];
    digits[kHundredsDigit] = static_cast<i16>(amount / kHundreds);
    amount -= kHundreds * digits[kHundredsDigit];
    digits[kTensDigit] = static_cast<i16>(amount / kTens);
    digits[kOnesDigit] = static_cast<i16>(amount - static_cast<u16>(static_cast<u16>(digits[kTensDigit]) * kTens));
}

// swd32.exe 0x0047E4C0 (unmatched in swd3e): the effects after the head node
int BattleUnit::HasEffectElement(i16 element) const
{
    if (element == 0)
    {
        return 0;
    }

    for (const EffectNode* node = this->effects.next.get(); node != nullptr; node = node->next.get())
    {
        if (element == static_cast<i16>(node->element))
        {
            return 1;
        }
    }

    return 0;
}

// swd3e.exe 0x00493D50: the effects after the head node
int BattleUnit::HasEffectBits(u16 bits) const
{
    for (const EffectNode* node = this->effects.next.get(); node != nullptr; node = node->next.get())
    {
        if ((node->bits & bits) != 0)
        {
            return 1;
        }
    }

    return 0;
}

// swd3e.exe 0x00493E20: applies the pending damage (bit 15: healing) with its number; true when the unit fell
bool BattleUnit::ApplyPendingDamage(BattleServices& services)
{
    if (this->pendingDamage == 0)
    {
        return false;
    }

    this->PushTarget(BattleUnit::DamageTarget);

    if ((this->pendingDamage & BattleUnit::Healing) != 0)
    {
        this->PushTarget(BattleUnit::HealTarget);
    }

    this->SetDamageDigits(services, static_cast<i16>(this->pendingDamage));
    this->StepTargetCount(1);

    if ((this->pendingDamage & BattleUnit::Healing) == 0)
    {
        this->RaiseFlag26B8(services.state);
    }

    const int result = this->Damage(services, static_cast<i16>(this->pendingDamage), 0, 0);
    this->pendingDamage = 0;
    return result == 1;
}

// swd3e.exe 0x004946E0: the icons (picture 0x23B6, frame icon - 1) of the effects in a row, the head node first
void BattleUnit::DrawEffectIcons(BattleServices& services, int x, int y)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).DrawEffectIconsAdvance(services, x, y, scene);
    scene.Show(services);
}

// swd3e.exe 0x00495330: applies hit point damage and spirit and stamina changes; 1 when the unit falls. A party role
// with the gold guard pays the damage from the gold while it is not zero (the gold can go below zero); an enemy with an
// override loses that first.
int BattleUnit::Damage(BattleServices& services, int hitPoints, i16 spirit, i16 stamina)
{
    if (hitPoints > 0 && ((this->marks & BattleUnit::UnkMark8000) != 0 || (this->status & BattleUnit::ResistAll) != 0))
    {
        return 0;
    }

    if (this->defeated == 1 || this->EffectFlag3() == 1)
    {
        return 0;
    }

    if (this->kind != BattleUnit::PartyRole)
    {
        ObjectData& block = this->data->data;
        const i32 shield = this->hitPointsOverride;

        if (shield != 0)
        {
            this->hitPointsOverride = hitPoints < shield ? shield - hitPoints : 0;

            if (this->hitPointsOverride > block.maxHitPoints)
            {
                this->hitPointsOverride = block.maxHitPoints;
            }

            if (this->hitPointsOverride > 0)
            {
                return 0;
            }

            block.hitPoints = 0;
            this->hitPointsOverride = 0;
        }
        else
        {
            const i16 life = block.hitPoints;
            block.hitPoints = static_cast<i16>(hitPoints < life ? life - hitPoints : 0);

            if (block.hitPoints < 0)
            {
                block.hitPoints = static_cast<i16>(block.maxHitPoints);
            }

            if (block.hitPoints > block.maxHitPoints)
            {
                block.hitPoints = static_cast<i16>(block.maxHitPoints);
            }

            if (block.hitPoints > 0)
            {
                return 0;
            }

            block.hitPoints = 0;
        }

        const u8 pose = this->defeatPose;
        this->dying = 1;
        this->glowLevel = 0;

        if (pose == 0)
        {
            this->defeatPose = static_cast<u8>(services.random.Below(kDefeatPoses) + 1);
        }

        return 1;
    }

    RoleRecord& record = *this->roleRecord;
    i32& gold = services.party.variables[Party::Gold];

    if (this->goldGuard == 1 && gold != 0)
    {
        gold -= hitPoints;
    }
    else
    {
        record.hitPoints = static_cast<i16>(record.hitPoints - hitPoints);

        if (record.hitPoints > record.maxHitPoints)
        {
            record.hitPoints = record.maxHitPoints;
        }
    }

    record.spirit = static_cast<i16>(record.spirit + spirit);

    if (record.spirit > record.maxSpirit)
    {
        record.spirit = record.maxSpirit;
    }

    record.stamina = static_cast<i16>(record.stamina + stamina);

    if (record.stamina > record.maxStamina)
    {
        record.stamina = record.maxStamina;
    }

    if (record.spirit <= 0)
    {
        record.spirit = 0;
    }

    if (record.stamina <= 0)
    {
        record.stamina = 0;
    }

    if (record.hitPoints > 0)
    {
        return 0;
    }

    record.hitPoints = 0;
    this->dying = 1;

    if (this->autoControl == 1)
    {
        this->defeatPose = kFallbackDefeatPose;
    }

    return 1;
}

// swd32.exe 0x0047F340 (unmatched in swd3e)
int BattleUnit::EffectFlag2() const
{
    return (this->equipmentFlags & BattleUnit::BladeReturn) != 0 ? 1 : 0;
}

// swd32.exe 0x0047F360 (unmatched in swd3e)
int BattleUnit::EffectFlag3() const
{
    return (this->equipmentFlags & BattleUnit::Unk36) != 0 ? 1 : 0;
}

// swd3e.exe 0x00495670: a fixed pose overrides the requested one
void BattleUnit::SetDefeatPose(u8 pose)
{
    this->defeatPose = pose;

    if (this->fixedPose != 0)
    {
        this->defeatPose = this->fixedPose;
    }
}

// swd3e.exe 0x00495690
void BattleUnit::SetDefeatPoseFixed(u8 pose)
{
    this->fixedPose = pose;
    this->defeatPose = pose;
}

// swd3e.exe 0x00491690: clears the unit between battles: its actions (the slot actions 0..7 and 10), placement,
// command and target lists, effects (all freed), a party role's block, and its state words; the targets back to -1
void BattleUnit::Reset(BattleServices& services)
{
    this->mainAct = ActState{};
    this->standAct = ActState{};
    this->castAct = ActState{};
    this->recoilAct = ActState{};
    this->attackAct = ActState{};
    this->unk598 = ActState{};
    this->slotActs[BattleUnit::SlotActs - 1] = ActState{};

    for (int slot = 0; slot < kSlotActsPerTurn; ++slot)
    {
        this->slotActs[static_cast<std::size_t>(slot)] = ActState{};
    }

    this->placement = UnitPlacement{};
    this->command = UnitCommand{};
    this->numberFades = {};
    this->pushedTargets = {};
    this->trails = {};
    this->unk283C = {};
    this->ExpireEffects(services, 1);

    if (this->kind == BattleUnit::PartyRole && this->data != nullptr)
    {
        this->ownedData.reset();
        this->data = nullptr;
    }

    this->targets = {BattleUnit::NoTarget, BattleUnit::NoTarget, BattleUnit::NoTarget, BattleUnit::NoTarget};
    this->unk29A2 = BattleUnit::NoUnk29A2;
    this->fadeLevel = BattleUnit::Opaque;
    this->brightLevel = 1;
    this->flashPhase = BattleUnit::FlashStart;
    this->flashFall = BattleUnit::FlashPeak;
    this->slotTint = 0;
    this->unk2660 = 0;
    this->unk2664 = 0;
    this->marks = 0;
    this->status = 0;
    this->guardPose = 0;
    this->glowLevel = 0;
    this->action = 0;
    this->unk29A4 = 0;
    this->actId = 0;
    this->gauge = 0;
    this->unk2A14 = 0;
    this->glowPhase = 0;
    this->commandA = 0;
    this->unk2A6E = 0;
    this->unk2A72 = 0;
    this->unk2A74 = 0;
    this->hurtStep = 0;
    this->unk2A7A = 0;
    this->targetCount = 0;
    this->queuedActions = 0;
    this->damageSlot = 0;
    this->landedActions = 0;
    this->unk26B8 = 0;
    this->healChoice = 0;
    this->unk2A92 = 0;
    this->evadeSkill = 0;
    this->defeatPose = 0;
    this->fixedPose = 0;
    this->commandFlags = 0;
    this->idleTurns = 0;
    this->itemWait = 0;
    this->skillWait = 0;
    this->choicePhase = 0;
    this->pendingDamage = 0;
    this->equipmentFlags = 0;
    this->unk26CC = 0;
    this->kind = 0;
    this->unk2AA4 = 0;
    this->unk2AA8 = 0;
    this->unk2AAC = 0;
    this->unk2AB0 = 0;
    this->unk2AB4 = 0;
    this->defeated = 0;
    this->outline = 0;
    this->charging = 0;
    this->unk2AC8 = 0;
    this->stage = 0;
    this->held = 0;
    this->unk2AD4 = 0;
    this->unk2AD8 = 0;
    this->unk2ADC = 0;
    this->unk2AE0 = 0;
    this->unk2A8C = 0;
    this->target = 0;
    this->extraNumber = 0;
    this->unk2AEC = 0;
    this->guarding = 0;
    this->unk2AF4 = 0;
    this->dying = 0;
    this->unk2ACC = 0;
    this->unk2AF8 = 0;
    this->absorbing = 0;
    this->autoControl = 0;
    this->guestControl = 0;
    this->facing = 0;
    this->unk2B0C = 0;
    this->facingTurnsBack = 0;
}

// swd3e.exe 0x00495BF0
int BattleUnit::SetFacing(int value)
{
    this->facing = value;
    return value;
}

// swd3e.exe 0x0049B170
void BattleUnit::SetDisplayName(std::span<const char> name)
{
    this->displayName = name;
}

// swd3e.exe 0x0049A9C0: a party role's maximum stamina and its stamina (nothing for an enemy)
void BattleUnit::GetStamina(u16& maximum, u16& value) const
{
    if (this->kind == BattleUnit::PartyRole)
    {
        maximum = static_cast<u16>(this->roleRecord->maxStamina);
        value = static_cast<u16>(this->roleRecord->stamina);
    }
}

// swd3e.exe 0x0049A9F0: a party role's maximum spirit and its spirit (nothing for an enemy)
void BattleUnit::GetSpirit(u16& maximum, u16& value) const
{
    if (this->kind == BattleUnit::PartyRole)
    {
        maximum = static_cast<u16>(this->roleRecord->maxSpirit);
        value = static_cast<u16>(this->roleRecord->spirit);
    }
}

// swd3e.exe 0x0049B680: the hit points and their maximum: a party role's from its record, an enemy's from its object
// (its current hit points when overridden)
void BattleUnit::GetHitPoints(i32& value, i32& maximum) const
{
    if (this->kind == BattleUnit::PartyRole)
    {
        maximum = this->roleRecord->maxHitPoints;
        value = this->roleRecord->hitPoints;
        return;
    }

    value = this->data->data.hitPoints;

    if (this->hitPointsOverride != 0)
    {
        value = this->hitPointsOverride;
    }

    maximum = this->data->data.maxHitPoints;
}

// swd32.exe 0x004755E0 (unmatched in swd3e): the action gauge filled by a quarter of the agility (an eighth while
// slowed, with a quarter
// more and one when boosted; speed up and speed down add and take 30%, invisibility takes 10% more) while below full;
// a full gauge marks the unit ready (true). Held units do not move.
int BattleUnit::AdvanceGauge(const BattleServices& services, int boost)
{
    if ((this->marks & BattleUnit::Held) != 0 || (this->marks & BattleUnit::Unk4000) != 0)
    {
        return 0;
    }

    const u16 gauge = this->gauge;

    // 0x0047561B: the zero-extended gauge against the dword 0x004A74CC, signed (jl)
    if (static_cast<i32>(gauge) < services.state.gaugeFull)
    {
        int step = this->data->data.agility >> kAgilityShift;

        if ((this->status & BattleUnit::Slowed) != 0)
        {
            step = this->data->data.agility >> kSlowedAgilityShift;
        }

        if (boost == 1)
        {
            step += step / kBoostDivisor + 1;
        }

        int gain = 0;
        int loss = 0;

        if ((this->status & BattleUnit::SpeedUp) != 0)
        {
            gain = kSpeedChangePercent * step / kPercent;
        }

        if ((this->status & BattleUnit::SpeedDown) != 0)
        {
            loss = kSpeedChangePercent * step / kPercent;
        }

        if ((this->status & BattleUnit::Invisible) != 0)
        {
            loss = static_cast<u16>(kInvisibleSlowPercent * step / kPercent + loss);
        }

        this->unk2AB0 = 0;
        this->gauge = static_cast<u16>(step + gain + gauge - loss);
        return 0;
    }

    this->unk2AB0 = 1;
    this->unk2AEC = 0;

    if (this->facingTurnsBack == 1)
    {
        this->facingTurnsBack = 0;
        this->facing = 0;
    }

    return 1;
}

// The command block read as the skill script it was loaded from (the same 0x28-byte record in the original).
SkillScript BattleUnit::CommandSkill() const
{
    SkillScript skill;
    skill.unk00 = static_cast<u32>(this->command.unk00);
    skill.flags = this->command.actionFlags;
    skill.statusBits = static_cast<u32>(this->command.unk08);
    skill.unk0C = MakeWords(static_cast<u16>(this->command.value), this->command.flags);
    skill.unk10 = MakeWords(this->command.followAct, this->command.resistance);
    skill.unk14 = this->command.castAct;
    skill.unk16 = this->command.unk16;
    skill.unk18 = this->command.unk18;
    skill.ailment = this->command.ailment;
    skill.element = this->command.element;
    skill.unk1E = this->command.pose;
    skill.unk20 = static_cast<u16>(this->command.unk1E);
    skill.unk22 = this->command.unk20;
    skill.unk24 = this->command.unk24;
    skill.unk26 = this->command.target;
    skill.unk28 = this->command.unk28;
    skill.turns = this->command.turns;
    return skill;
}

// The skill script `id` read into the command (0x00476A80 on the record at +0xD90; it sets only the fields its
// opcodes name).
void BattleUnit::LoadCommand(Database& database, u16 id)
{
    SkillScript skill = this->CommandSkill();
    database.LoadSkill(skill, id);
    this->command.unk00 = static_cast<i32>(skill.unk00);
    this->command.actionFlags = skill.flags;
    this->command.unk08 = static_cast<i32>(skill.statusBits);
    this->command.flags = static_cast<u16>(skill.unk0C);
    this->command.value = static_cast<i16>(HighWord(skill.unk0C));
    this->command.resistance = LowWord(skill.unk10);
    this->command.followAct = HighWord(skill.unk10);
    this->command.castAct = skill.unk14;
    this->command.unk16 = skill.unk16;
    this->command.unk18 = skill.unk18;
    this->command.ailment = skill.ailment;
    this->command.element = skill.element;
    this->command.pose = skill.unk1E;
    this->command.unk1E = static_cast<i16>(skill.unk20);
    this->command.unk20 = skill.unk22;
    this->command.unk24 = skill.unk24;
    this->command.target = skill.unk26;
    this->command.unk28 = skill.unk28;
    this->command.turns = skill.turns;
}

// swd3e.exe 0x0049D3B0
ObjectRecord* BattleUnit::Data() const
{
    return this->data;
}

// swd3e.exe 0x00483490
bool BattleUnit::MarkBit11Clear() const
{
    return (this->marks & BattleUnit::UnkMark800) == 0;
}

// swd3e.exe 0x00495C10: held (by the mark)
int BattleUnit::IsHeld() const
{
    return (this->marks & BattleUnit::Held) != 0 ? 1 : 0;
}

// swd3e.exe 0x00495C00
u16 BattleUnit::ActId() const
{
    return this->actId;
}

// swd3e.exe 0x00499EC0: the drain of the command: an attack drain (bit 8) drains life with a vampire ring (8) and hit
// points otherwise (1); bit 9 drains spirit (2); bit 10 stamina (4)
u16 BattleUnit::DrainKind() const
{
    const u32 flags = this->command.actionFlags;

    if ((flags & UnitCommand::DrainsLife) != 0)
    {
        return (this->equipmentFlags & BattleUnit::LifeDrain) != 0 ? u16{BattleUnit::DrainedLife}
                                                                   : u16{BattleUnit::DrainedHitPoints};
    }

    if ((flags & UnitCommand::DrainsSpirit) != 0)
    {
        return BattleUnit::DrainedSpirit;
    }

    return (flags & UnitCommand::DrainsStamina) != 0 ? u16{BattleUnit::DrainedStamina} : u16{0};
}

// swd3e.exe 0x0049A950
int BattleUnit::StatusBit5() const
{
    return (this->status & BattleUnit::UnkStatus20) != 0 ? 1 : 0;
}

// swd3e.exe 0x00498CD0
int BattleUnit::MarkBit4() const
{
    return (this->marks & BattleUnit::UnkMark10) != 0 ? 1 : 0;
}

// swd3e.exe 0x0049C2C0: unk2EB2 plus one (the original adds to the zero-extended word: its caller's eax has a clear
// high word)
i32 BattleUnit::Unk2EB2Shown() const
{
    return static_cast<i32>(static_cast<u16>(this->unk2EB2)) + 1;
}

// swd3e.exe 0x0049C2D0: the proficiency of accessory kind kUnk2EB2Accessory taken, whether unk2EB2 is below a tenth of
// it
bool BattleUnit::Unk2EB2BelowProficiency(CounterList& proficiency)
{
    this->AccessoryProficiency(kUnk2EB2Accessory, proficiency);
    return this->unk2EB2 < static_cast<i32>(this->proficiency / kUnk2EB2Share);
}

// swd3e.exe 0x0049B160
int BattleUnit::SetValueAAC(int value)
{
    this->unk2AAC = value;
    return value;
}

// swd3e.exe 0x0049B180: the facing (and +0x2B20) set, unless the unit is a defeated party role
int BattleUnit::SetFacingIfAble(int value)
{
    if (this->kind != BattleUnit::PartyRole || this->defeated != 1)
    {
        this->facingTurnsBack = value;
        this->facing = value;
        return value;
    }

    return 1;
}

// swd32.exe 0x00482080 (unmatched in swd3e)
void BattleUnit::SetNudge(u16 nudge, u16 saved)
{
    this->nudge = nudge;
    this->drawAtSaved = saved;
}

// swd3e.exe 0x0049A970: the two values of a command that names a target (flag 1)
bool BattleUnit::CommandTarget(u16& target, u16& second) const
{
    if ((this->command.flags & UnitCommand::TargetsUnit) == 0)
    {
        return false;
    }

    target = this->command.target;
    second = this->command.unk28;
    return true;
}

// swd3e.exe 0x00498CC0
int BattleUnit::MarkBit1() const
{
    return (this->marks >> 1) & 1;
}

// swd3e.exe 0x00498CB0
bool BattleUnit::MarkBit8Clear() const
{
    return (this->marks & BattleUnit::UnkMark100) == 0;
}

// swd3e.exe 0x00499EB0: the role's battle block starts with 0x30
bool BattleUnit::HasBlock30() const
{
    return *this->roleBlock == kUnkBlock30;
}

// swd3e.exe 0x00482350
int BattleUnit::Word2A86Bit13() const
{
    return (this->commandFlags & BattleUnit::UnkCommandFlag2000) != 0 ? 1 : 0;
}

// swd3e.exe 0x00482360
int BattleUnit::Word2A86Bit12() const
{
    return (this->commandFlags & BattleUnit::UnkCommandFlag1000) != 0 ? 1 : 0;
}

// swd32.exe 0x00482030 (unmatched in swd3e): the main act started over (cleared, the cast act marked done and +0x26D2
// set) when it waits or
// when forced
bool BattleUnit::RestartMainAct(i16 value, int force)
{
    if (this->mainAct.waiting != 1 && force != 1)
    {
        return false;
    }

    this->mainAct = ActState{};
    this->castAct.waiting = 1;
    this->guardPose = value;
    return true;
}

// swd32.exe 0x00476140 (unmatched in swd3e): the command sets status bit 28, or its flag 12
int BattleUnit::CommandStatusBit() const
{
    if ((static_cast<u32>(this->command.unk08) & kCommandStatusBit) != 0)
    {
        return 1;
    }

    return (this->command.actionFlags & UnitCommand::UnkAction1000) != 0 ? 1 : 0;
}

// swd3e.exe 0x00489300
void BattleUnit::CommandValues(u16& value, u16& turns) const
{
    value = this->command.unk20;
    turns = this->command.turns;
}

// swd3e.exe 0x004892D0: a machine enemy (kind 0x800, like 蟹精魔機) of a type up to 0xA, never a named one
bool BattleUnit::IsSmallMachine() const
{
    const u32 kind = this->data->data.kind;

    if ((kind & BattleEnemy::NamedKind) != 0)
    {
        return false;
    }

    if ((kind & BattleEnemy::MachineKind) != 0)
    {
        return this->data->data.category <= BattleEnemy::SmallMachineTypes;
    }

    return false;
}

// swd32.exe 0x00475870 (unmatched in swd3e)
void BattleUnit::Unk62And8A(u16& first, u16& second) const
{
    first = this->data->data.unk66;
    second = this->data->data.unk90;
}

// swd3e.exe 0x00499FA0: the value (+0x40, low 14 bits) of the worn accessory of `kind`, 0 without one (and for kind
// 0x28 under the UnkEquipment100 flag)
u16 BattleUnit::AccessoryValue(u16 kind) const
{
    for (const ObjectRecord& accessory : this->accessories)
    {
        if (accessory.data.unk48 == kind)
        {
            if (accessory.data.unk48 == kUnkAccessory28 && (this->equipmentFlags & BattleUnit::UnkEquipment100) != 0)
            {
                return 0;
            }

            return static_cast<u16>(accessory.data.costBits & kAccessoryValueMask);
        }
    }

    return 0;
}

// swd3e.exe 0x00499F40: the proficiency in percent of the worn accessory of `kind` (kept at +0x26DC; the last one
// kept without such an accessory)
i16 BattleUnit::AccessoryProficiency(u16 kind, CounterList& proficiency)
{
    for (const ObjectRecord& accessory : this->accessories)
    {
        if (accessory.data.unk48 == kind)
        {
            this->proficiency = static_cast<u16>(FindCounterPercent(proficiency, static_cast<i16>(accessory.data.act)));
            return static_cast<i16>(this->proficiency);
        }
    }

    return static_cast<i16>(this->proficiency);
}

// swd3e.exe 0x004893C0: the object's words +0x78..+0x7C set where non-zero (the fourth value goes to +0x7A as well)
void BattleUnit::SetWords72(i16 first, i16 second, i16 third, i16 fourth)
{
    ObjectData& object = this->data->data;

    if (first != 0)
    {
        object.skillA = static_cast<u16>(first);
    }

    if (second != 0)
    {
        object.skillACount = static_cast<u16>(second);
    }

    if (third != 0)
    {
        object.skillB = static_cast<u16>(third);
    }

    if (fourth != 0)
    {
        object.skillACount = static_cast<u16>(fourth);
    }
}

// swd3e.exe 0x00489350: the object's words +0x66..+0x70 set where non-zero
void BattleUnit::SetWords66(i16 a, i16 b, i16 c, i16 d, i16 e, i16 f)
{
    ObjectData& object = this->data->data;
    const std::array<std::pair<i16, u16*>, 6> words = {{
        {a, &object.itemA},
        {b, &object.itemACount},
        {c, &object.itemB},
        {d, &object.itemBCount},
        {e, &object.itemC},
        {f, &object.itemCCount},
    }};

    for (const auto& [value, word] : words)
    {
        if (value != 0)
        {
            *word = static_cast<u16>(value);
        }
    }
}

// swd32.exe 0x00476920 (unmatched in swd3e): the object's resistances from the low bytes of nine words
void BattleUnit::SetResistances(std::span<const i16, 9> values)
{
    for (std::size_t element = 0; element < this->data->data.resistances.size(); ++element)
    {
        this->data->data.resistances[element] = static_cast<u8>(values[element]);
    }
}

// swd3e.exe 0x00499EF0: the unit has an action and its main act shows picture `picture` - 1 (-1: the main act waits)
bool BattleUnit::ShowsPicture(i16 picture) const
{
    if (this->action == 0)
    {
        return false;
    }

    if (picture != -1)
    {
        // 0x00482EF4: the zero-extended picture against the sign-extended picture - 1
        return static_cast<int>(this->mainAct.picture) == picture - 1;
    }

    return this->mainAct.waiting == 1;
}

// swd3e.exe 0x00488BB0: skill `id` of mon.dat taken as the unit's action: its act, its name, its command script (the
// command's +0x0E adding to +0x29A4) and command 2
void BattleUnit::LoadSkillAction(Database& database, u16 id, u16& act)
{
    database.LoadObject(this->object, id);
    act = this->object.data.act;
    const std::span<const char> name(this->object.data.name);
    const std::size_t length = std::min<std::size_t>(strnlen(name.data(), name.size()), this->skillName.size() - 1);
    std::copy_n(name.begin(), length, this->skillName.begin());
    this->skillName[length] = '\0';
    this->LoadCommand(database, this->object.data.skillScript);
    this->unk29A4 = static_cast<u16>(this->unk29A4 + static_cast<u16>(this->command.value));
    this->SetCommand(BattleUnit::UnkCommand2);
    this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
}

// swd3e.exe 0x00488C20: an enemy's command: with +0x2A8C a command of flag 2 runs (command 2, its cast act returned);
// otherwise the enemy's own skill script is read afresh, +0x29A4 starts from its attack and command 1 follows
u16 BattleUnit::PrepareCommand(Database& database)
{
    if (this->unk2A8C != 0)
    {
        if ((this->command.flags & UnitCommand::UnkFlag2) == 0)
        {
            return 0;
        }

        this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
        this->SetCommand(BattleUnit::UnkCommand2);
        return this->command.castAct;
    }

    this->unk29A4 = 0;
    this->command = UnitCommand{};
    this->LoadCommand(database, this->data->data.skill);
    this->unk29A4 = static_cast<u16>(this->unk29A4 + this->data->data.attack);
    this->SetCommand(1);
    return 0;
}

// swd32.exe 0x00476080 (unmatched in swd3e): an enemy below a third of its hit points heals itself half the time (on a
// roll of 0..4 its
// first way, 5..9 its second) when it has that way, may heal and is not marked otherwise
bool BattleUnit::ChooseHeal(BattleServices& services)
{
    u16 roll = static_cast<u16>(services.random.Below(kHealRoll));

    if (roll < kHealRoll / 2)
    {
        roll = 0;
    }
    else if (roll < kHealRoll)
    {
        roll = 1;
    }

    const ObjectData& object = this->data->data;
    i32 hitPoints = object.hitPoints;

    if (this->hitPointsOverride != 0)
    {
        hitPoints = this->hitPointsOverride;
    }

    if (hitPoints >= object.maxHitPoints / kHealBelowShare)
    {
        return false;
    }

    const bool free = (this->marks & BattleUnit::UnkMark100) == 0 && object.itemChance != 0;

    if (roll == 0)
    {
        if (object.healCountA != 0 && free)
        {
            this->healChoice = kFirstHeal;
            return true;
        }
    }
    else if (roll == 1 && object.healCountB != 0 && free)
    {
        this->healChoice = kSecondHeal;
        return true;
    }

    return false;
}

// swd3e.exe 0x00498CE0: a command that does not move across (flag 4) starts at x 0, or 0x280 for a target command of
// an enemy or of a role facing left; one that does not move down (flag 5) at y 0
void BattleUnit::ClearMoveAxes(u16& x, u16& y) const
{
    if ((this->command.actionFlags & UnitCommand::StaysAcross) != 0)
    {
        x = 0;

        if ((this->command.flags & UnitCommand::TargetsUnit) != 0)
        {
            if (this->kind == BattleUnit::PartyRole && this->facing == 1)
            {
                x = Display::Width;
            }

            if (this->kind == 0)
            {
                x = Display::Width;
            }
        }
    }

    if ((this->command.actionFlags & UnitCommand::StaysDown) != 0)
    {
        y = 0;
    }
}

// swd3e.exe 0x00485AD0: a roll of 0..99 at most 70% of `percent` plus 10 (16-bit compare)
bool RollChance(Random& random, int percent)
{
    return static_cast<u16>(kChanceScale * percent / kPercent + kChanceBase) >=
           static_cast<u16>(random.Below(kPercent));
}

// swd3e.exe 0x00497900: the damage of this unit's hit on `target` (negative: healing; -1 shows no number), with the
// spirit and stamina changes it makes. A barrier stops a hit that does not restore. The buff and ailment of the command
// land first (a held target is freed by a hit that restores nothing). The target's defence and resistances are those
// of its party record or its mon.dat object. A restoring command (actionFlags Restores) cures and heals at once; an
// attack takes the attack (or the battle record's bonus) plus a roll on the level, raised and cut by the attack buffs,
// less the defence, scaled when enraged and by the equipment, then by the resistance of the command's element. In a
// scripted battle the attacker's act adds a special effect. Skills take their value instead. The original computes
// the damage in 16 bits.
i16 BattleUnit::HitDamage(BattleServices& services, BattleUnit& target, i16& spiritDamage, i16& staminaDamage)
{
    i16 attack = static_cast<i16>(this->unk29A4);
    i32 healHitPoints = 0;
    i32 healSpirit = 0;
    i32 healStamina = 0;
    i32 raise = 0;
    i32 cut = 0;
    bool resisted = false;
    const RoleRecord* targetRecord = nullptr;
    const ObjectRecord* targetData = nullptr;
    u16 defense = 0;
    std::array<u8, kResistances> resistances = {};
    spiritDamage = 0;
    staminaDamage = 0;
    target.PushTarget(BattleUnit::DamageTarget);
    const bool restores = (this->command.actionFlags & UnitCommand::Restores) != 0;

    if (!restores && target.HasEffectBits(BattleUnit::UnkStatus4) == 1)
    {
        return 0;
    }

    const bool anyHeal = this->restoreHitPoints != 0 || this->restoreSpirit != 0 || this->restoreStamina != 0;

    if (target.AddBuff(services, this->CommandSkill()) == 1 && this->restoreHitPoints == 0 && !anyHeal)
    {
        return -1;
    }

    if (!anyHeal && target.IsHeld() == 1)
    {
        target.CureEffects(services, BattleUnit::UnkStatus8);
    }

    auto resistanceOf = [&resistances](u16 element)
    {
        return static_cast<i8>(resistances[static_cast<std::size_t>(element - 1)]);
    };

    auto elemental = [](u16 element)
    {
        return element >= 1 && element <= kResistances;
    };

    if (target.kind == BattleUnit::PartyRole)
    {
        targetRecord = static_cast<const RoleRecord*>(target.SelectEntry(0));
        defense = targetRecord->defenseBonus;
        std::copy_n(targetRecord->resistances.begin(), resistances.size(), resistances.begin());

        if (static_cast<u16>(services.random.Below(kAilmentRoll)) > static_cast<u16>(targetRecord->ailmentChance))
        {
            target.AddAilment(services, this->CommandSkill());
        }

        target.SetDefeatPose(LowByte(this->command.pose));
    }
    else
    {
        targetData = target.Data();
        defense = targetData->data.defense;
        std::copy_n(
            targetData->data.resistances.begin(),
            std::min(resistances.size(), targetData->data.resistances.size()),
            resistances.begin()
        );

        if (this->command.ailment != 0)
        {
            if (this->command.value != 0 && elemental(this->command.resistance))
            {
                ScaleByTenths(resistanceOf(this->command.resistance), this->command.value);
            }

            if (target.AddAilment(services, this->CommandSkill()) == 1)
            {
                target.RaiseFlag26B8(services.state);
            }
            else if (this->command.value == 0)
            {
                target.RollEvade(services, kEvadeRoll);
            }

            if (this->command.ailment != 0 && this->command.value == 0)
            {
                return -1;
            }
        }

        target.SetDefeatPose(LowByte(this->command.pose));
    }

    if (this->healPercent == 1)
    {
        if (targetRecord != nullptr)
        {
            healHitPoints = static_cast<i16>(this->restoreHitPoints) * targetRecord->maxHitPoints / kPercent;
            healSpirit = targetRecord->maxSpirit * static_cast<i16>(this->restoreSpirit) / kPercent;
            healStamina = targetRecord->maxStamina * static_cast<i16>(this->restoreStamina) / kPercent;
        }
    }
    else
    {
        healHitPoints = this->restoreHitPoints;
        healSpirit = this->restoreSpirit;
        healStamina = this->restoreStamina;
    }

    if (restores)
    {
        target.CureEffects(services, this->command.ailment);
        spiritDamage = static_cast<i16>(healSpirit);
        staminaDamage = static_cast<i16>(healStamina);

        if (LowWord(static_cast<u32>(healHitPoints)) == 0)
        {
            return -1;
        }

        target.PushTarget(BattleUnit::HealTarget);
        return static_cast<i16>(-healHitPoints);
    }

    const u16 kind = this->commandA;

    if (kind == BattleUnit::UnkCommand1 || kind == BattleUnit::UnkCommand1A || kind == BattleUnit::UnkCommand18)
    {
        if ((this->unk26CC & kAttackFromBonus) != 0)
        {
            attack = static_cast<i16>(this->battleRecord->attackBonus);
        }

        if ((this->autoControl == 1 || this->guestControl == 1) && this->command.value == 0)
        {
            attack = static_cast<i16>(this->battleRecord->attackBonus);
        }

        auto levelRoll = [this, &services]
        {
            const auto limit = static_cast<u32>(this->battleRecord->level / kLevelShare + kLevelRollBase);
            return static_cast<i32>(services.random.Below(limit)) + 1;
        };

        attack = static_cast<i16>(attack + levelRoll());

        if (this->HasEffectKind(static_cast<i16>(this->command.resistance)) == 1)
        {
            raise = kAttackBuffPercent * attack / kPercent;
        }

        if ((this->status & BattleUnit::AttackUp) != 0)
        {
            raise += kAttackBuffPercent * attack / kPercent;
        }

        if ((this->status & BattleUnit::AttackDown) != 0 && (this->unk26CC & kAttackDownIgnored) == 0)
        {
            cut = kAttackBuffPercent * attack / kPercent;
        }

        attack = static_cast<i16>(attack + (raise - cut));

        if (services.state.criticalAttack == 1 && this->autoControl == 0 && this->guestControl == 0)
        {
            attack = static_cast<i16>(static_cast<i16>(this->battleRecord->attackBonus) + levelRoll());
            u8 pose = 0;

            switch (this->placement.actId)
            {
                case kFallAct:
                    pose = kFallPose;
                    break;

                case kDrainAct:
                {
                    // A party role as the target has no object: the original reads a null pointer here.
                    if (targetData == nullptr || (targetData->data.kind & ObjectData::UnkKind20) != 0)
                    {
                        break;
                    }

                    i32 hitPoints = 0;
                    i32 maximum = 0;
                    target.GetHitPoints(hitPoints, maximum);

                    if (static_cast<i32>(defense) + hitPoints >= attack)
                    {
                        attack = static_cast<i16>(hitPoints + defense);
                    }

                    target.SetVitals(0, 0, 0);
                    target.SetDefeatPose(kDrainPose);
                    pose = kDrainPoseAfter;
                    break;
                }

                case kWeakAct:
                    attack = static_cast<i16>(kWeakPercent * attack / kPercent);
                    pose = kWeakPose;
                    break;

                case kStrongAct:
                {
                    attack = static_cast<i16>(kStrongPercent * attack / kPercent);

                    if (targetData == nullptr || (targetData->data.kind & ObjectData::UnkKind20) != 0)
                    {
                        break;
                    }

                    const u32 roll = services.random.Below(kStrongAilments);

                    if (roll == 0)
                    {
                        this->command.ailment = SkillScript::UnkAilment100;
                    }
                    else if (roll == 1)
                    {
                        this->command.ailment = SkillScript::SlowedAilment;
                    }
                    else if (roll == kThirdAilment)
                    {
                        this->command.ailment = SkillScript::UnkAilment1;
                    }

                    this->command.turns = kStrongTurns;
                    target.AddAilment(services, this->CommandSkill());
                    pose = kStrongPose;
                    break;
                }

                default:
                    break;
            }

            if (pose != 0)
            {
                target.SetDefeatPose(pose);
            }
        }

        attack = attack > static_cast<i32>(defense) ? static_cast<i16>(attack - defense) : i16{1};

        if ((this->unk26CC & kAttackFromBonus) != 0)
        {
            const i32 value = attack;
            const auto share = static_cast<u16>(kEnragedSharePercent * value / kPercent);
            attack = static_cast<i16>(kEnragedPercent * value / kPercent + share * this->proficiency / kPercent);
        }

        if ((this->equipmentFlags & BattleUnit::BladeReturn) != 0)
        {
            attack = static_cast<i16>(crt::Ftol(static_cast<double>(attack) * kBladeScale));
        }

        if (elemental(this->command.resistance))
        {
            if (this->command.value == 0)
            {
                resisted = ScaleByTenths(resistanceOf(this->command.resistance), attack) == 1;
            }
            else if (this->autoControl == 0)
            {
                ScaleByTenths(resistanceOf(this->command.resistance), this->command.value);
                attack = static_cast<i16>(attack + static_cast<u16>(this->command.value));
                resisted = ScaleByTenths(static_cast<i8>(resistances.back()), attack) == 1;
            }
        }
    }

    if (kind == BattleUnit::UnkCommand1B)
    {
        const i32 scale = kSkillScaleShare * this->proficiency / kPercent + 1;
        attack = static_cast<i16>(
            attack * scale + static_cast<u16>(this->battleRecord->strength * scale) + this->battleRecord->level
        );
    }

    if (kind == BattleUnit::UnkCommand1F)
    {
        return (target.marks & BattleUnit::UnkMark8000) != 0 ? i16{0} : attack;
    }

    if (kind == BattleUnit::UnkCommand2 || (this->commandFlags & BattleUnit::UnkCommandFlag200) != 0 ||
        kind == BattleUnit::UnkCommand16 || kind == BattleUnit::UnkCommand3)
    {
        attack = this->command.value;

        if (this->HasEffectKind(static_cast<i16>(this->command.resistance)) == 1)
        {
            attack = static_cast<i16>(attack + kAttackBuffPercent * attack / kPercent);
        }

        if ((this->commandFlags & BattleUnit::UnkCommandFlag4000) == 0)
        {
            const auto divisor = static_cast<u16>(services.random.Below(kSkillRollDivisors) + kSkillRollBase);
            const i32 spread = this->battleRecord->level + static_cast<u16>(this->battleRecord->wisdom);
            const auto limit = static_cast<u16>(spread / divisor);
            const i32 roll = static_cast<i32>(services.random.Below(limit)) + 1;

            if (attack > 0)
            {
                attack = static_cast<i16>(attack + roll);
            }
        }

        spiritDamage = static_cast<i16>(healSpirit);
        staminaDamage = static_cast<i16>(healStamina);

        if (elemental(this->command.resistance) && ScaleByTenths(resistanceOf(this->command.resistance), attack) == 1)
        {
            resisted = true;
        }
    }

    if (target.HasEffectElement(static_cast<i16>(this->command.element)) == 1)
    {
        attack = static_cast<i16>(attack / kHalf);
    }

    const bool healsHitPoints = static_cast<i16>(healHitPoints) != 0;

    if (healsHitPoints || kind == BattleUnit::UnkCommand22)
    {
        target.PushTarget(BattleUnit::HealTarget);
        attack = static_cast<i16>(-healHitPoints);
    }

    if (kind == BattleUnit::UnkCommand23)
    {
        spiritDamage = static_cast<i16>(healSpirit);
        target.PushTarget(BattleUnit::SpiritGainTarget);

        if (static_cast<i16>(healSpirit) == 0)
        {
            attack = -1;
        }
    }

    if (kind == BattleUnit::UnkCommand24)
    {
        staminaDamage = static_cast<i16>(healStamina);
        target.PushTarget(BattleUnit::StaminaGainTarget);
    }

    if ((static_cast<i16>(healSpirit) != 0 || static_cast<i16>(healStamina) != 0) && !healsHitPoints)
    {
        attack = -1;
    }

    if (resisted)
    {
        target.PushTarget(BattleUnit::HealTarget);
    }

    if (target.GetGuard() == 1 && !healsHitPoints && this->command.resistance != 0)
    {
        attack = static_cast<i16>(attack - attack / kGuardShare);
    }

    if (target.kind == BattleUnit::PartyRole && (targetRecord->condition & RoleRecord::Defeated) != 0)
    {
        spiritDamage = 0;
        attack = 0;
        staminaDamage = 0;

        if (static_cast<i16>(healSpirit) != 0)
        {
            target.PushTarget(BattleUnit::SpiritGainTarget);
        }

        if (static_cast<i16>(healStamina) != 0)
        {
            target.PushTarget(BattleUnit::StaminaGainTarget);
        }
    }

    return (target.marks & BattleUnit::UnkMark8000) != 0 ? i16{0} : attack;
}

// swd3e.exe 0x00495C20: one frame of `act` (`id`) flying from (fromX, fromY) to the target's draw position (from the
// target back with `backwards`; a zero fromY keeps it level). The act is mirrored by the facing like the unit, its
// anchor measured from the picture's right edge. Each frame the flight takes flightSteps[slot] line steps, one more
// than the frame before, or with steps kSpeeding a doubling count (from 1, past 0x20 growing by 0x30). The act's sound
// plays, the picture is drawn, and 1 tells that the target's x was reached (the steps and the line are cleared).
int BattleUnit::FlyAct(
    BattleServices& services,
    const BattleUnit& target,
    ActState& act,
    int slot,
    i32 id,
    int fromX,
    int fromY,
    int steps,
    int backwards
)
{
    smooth::Scene scene;
    const int result =
        smooth::UnitFrame(*this).Fly(services, target, act, slot, id, fromX, fromY, steps, backwards, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x0049A220: one frame of `act` drawn over its shadow, on the target (a command with AtTarget) or at the
// unit's place with its outline. Mirrored by the facing like the unit (a forced draw also adds Additive); a nudged
// unit shakes, the nudge halving to 1 and staying there. The act's sound pans to the side of the screen it is on. An
// invisible unit draws a fainter shadow and smears the destination instead of the picture. The act's overlay picture
// follows, cleared once the act waits.
void BattleUnit::DrawAction(BattleServices& services, const BattleUnit& target, ActState& act)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).Action(services, target, act, scene);
    scene.Show(services);
}

// swd3e.exe 0x0049AA60: one frame of `act` placed on the target (mirrored by the facing like the unit), or with a
// shift and an anchored act at the act DrawAction placed, the act's anchor apart (shiftX only decides, shiftY adds).
// The act's sound pans to the target's side of the screen; the act's target bits start the quake and raise flag 26B8
// of the target.
void BattleUnit::AimAction(BattleServices& services, BattleUnit& target, ActState& act, i16 shiftX, i16 shiftY)
{
    u16 targetX = 0;
    u16 targetY = 0;
    act.hold = 0;

    if (this->held == 1)
    {
        act.hold = 1;
    }

    if (services.acts.Run(act) == 0)
    {
        return;
    }

    this->picture = services.archive.GetPicture(act.frame, act.picture);

    // A missing picture is read through a null pointer in the original.
    if (this->picture == nullptr)
    {
        return;
    }

    services.painter.SetSource(this->picture->data.data());
    this->shadowMode = act.flags;
    this->aimOffsetX = static_cast<u16>(act.x);
    this->aimAnchorX = act.anchorX;

    auto mirror = [this, &act]
    {
        this->shadowMode ^= Painter::Mirror;

        if (act.x != 0)
        {
            this->aimOffsetX = static_cast<u16>(this->picture->width - static_cast<u16>(act.x));
        }

        if (act.anchorX != 0)
        {
            this->aimAnchorX = static_cast<u16>(this->picture->width - act.anchorX);
        }
    };

    if ((this->autoControl == 1 || this->guestControl == 1) && this->facing == 1)
    {
        mirror();
    }

    if (this->facing == 1 && this->autoControl == 0 && this->guestControl == 0)
    {
        mirror();
    }

    target.GetDrawPosition(targetX, targetY);
    this->aimX = static_cast<u16>(targetX - this->aimOffsetX);
    this->aimY = static_cast<u16>(targetY - static_cast<u16>(act.y));

    if ((act.anchorX != 0 || act.anchorY != 0) && (shiftX != 0 || shiftY != 0))
    {
        this->aimX = static_cast<u16>(this->actX + this->actAnchorX - this->aimAnchorX);
        this->aimY = static_cast<u16>(this->actY - act.anchorY + shiftY);
    }

    this->ClearMoveAxes(this->aimX, this->aimY);
    services.audio.PlaySound(act.view, services.config.soundVolume);
    services.audio.SetSoundPan(act.view, static_cast<i16>(targetX) >= kPanCentre ? kFadePan : -kFadePan);
    act.view = 0;

    if ((act.target & ActState::Quakes) != 0)
    {
        services.state.quaking = 1;
        services.state.quakePhase = 0;
    }

    if ((act.target & ActState::UnkTarget4) != 0)
    {
        act.target = 0;
        target.RaiseFlag26B8(services.state);
    }
}

// swd3e.exe 0x0049ACC0: one frame of the recoil act `act` at the place DrawAction drew the unit's act, `y` lower (the
// act measured from its anchor, mirrored by the facing like the unit). Its sound pans to the unit's side of the
// screen. Once the act waits, it is drawn tinted by recoilTint, darkening by 2 a frame (not while held) until it
// reaches kRecoilFadeEnd: then recoilTint is cleared and 1 tells that the recoil is over.
int BattleUnit::DrawRecoil(BattleServices& services, ActState& act, i16 y)
{
    smooth::Scene scene;
    const int result = smooth::UnitFrame(*this).Recoil(services, act, y, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x0049AF40: AimAction for a facing unit only (its x always measured from the right edge), drawn at once
// with the shadow picture's palette as the draw argument; 1 once the act waits.
int BattleUnit::DrawAimed(BattleServices& services, BattleUnit& target, ActState& act, i16 shiftX, i16 shiftY)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DrawAimedAdvance(services, target, act, shiftX, shiftY, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x00498DF0: one frame of the trail: every second frame the images move back one place and the current
// picture (`frame`, `picture`, `mode` at x, y) is pushed in front at level 12 (24 when additive). Each image is drawn
// at its own y and fades by fadeStep (gone at 0): additive ones tinted by their level (blue fixed at -16), the others
// translucent at their level with the trail's tint under them while above 1. The original places the image at
// 12 + 30 per place for direction 0 (the image's x cancels), at x + y - 12 - 30 per place for Left, at x for InPlace.
void BattleUnit::DrawTrail(
    BattleServices& services,
    AfterimageTrail& trail,
    u16 frame,
    u16 picture,
    u32 mode,
    i16 x,
    i16 y
)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).Trail(services, trail, frame, picture, mode, x, y, scene);
    scene.Show(services);
}

// swd3e.exe 0x00495FF0: one frame of the queued acts (`action`, when not 0, given to each first); 1 when there are none
// (queuedActions cleared). Each act, mirrored by the facing, shakes on its line for its shake frames (its y from
// kShakeOffsets), then flies it (one step more a frame, doubling while speeding) with its trail; at the line's end its
// impact act (if any) plays to its end within the frame (the original runs the whole node again until it waits), the
// node is done and, unless the command is UnkAction1 or the impact is negative, the hit lands on `target`. Done nodes
// are freed. A shaking node takes the arrival of the node before it (the original's first is uninitialised).
int BattleUnit::RunQueuedActions(BattleServices& services, BattleUnit& target, i32 action)
{
    smooth::Scene scene;
    const int result = smooth::UnitFrame(*this).Queued(services, target, action, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x00496510: appends an act (`id`, `action`) flying from (fromX, fromY) to (toX, toY) with `steps` line
// steps a frame flown (1: speeding) after `shakeFrames` frames of shaking, its impact act and its QueuedAction::Flag
// bits (swd3e); counts it in queuedActions
void BattleUnit::AddQueuedAction(
    i32 id,
    i32 action,
    i16 fromX,
    i16 fromY,
    i16 toX,
    i16 toY,
    i16 steps,
    u16 shakeFrames,
    i32 impact,
    u8 flags
)
{
    std::unique_ptr<QueuedAction>* link = &this->actions;

    while (*link != nullptr)
    {
        link = &(*link)->next;
    }

    *link = std::make_unique<QueuedAction>();
    QueuedAction& node = **link;
    node.act.id = id;
    node.act.action = action;
    node.impact.id = impact;
    node.fromX = fromX;
    node.fromY = fromY;
    node.toX = toX;
    node.toY = toY;
    node.shakeFrames = shakeFrames;
    node.steps = steps;
    node.done = 0;
    node.flags = flags;
    ++this->queuedActions;
}

// swd3e.exe 0x004965D0: the last queued act leaves a trail (three images in place, tinted under them); its first call
// clears the images. The original reads the list head as a node when nothing is queued.
void BattleUnit::TrailLastQueuedAction(i32 tintRed, i32 tintGreen, i32 tintBlue)
{
    QueuedAction* last = this->actions.get();

    if (last == nullptr)
    {
        return;
    }

    while (last->next != nullptr)
    {
        last = last->next.get();
    }

    last->trailed = 1;
    AfterimageTrail& trail = last->trail;

    if (trail.started != 0)
    {
        return;
    }

    trail.started = 1;

    for (AfterimageTrail::Image& image : trail.images)
    {
        image.picture = AfterimageTrail::NoPicture;
    }

    trail.tintRed = tintRed;
    trail.tintGreen = tintGreen;
    trail.tintBlue = tintBlue;
    trail.direction = AfterimageTrail::InPlace;
    trail.count = kQueuedTrailImages;
}

// swd3e.exe 0x0049B3B0: DrawAction at (x, y) without the outline, the shake and the overlay: `act` over its shadow,
// mirrored like the unit (a forced draw adds Additive), the sound panned to its side. An invisible unit smears the
// destination with the act's own flags; a blending act (both bits of ActState::Signal) takes the main act's level.
void BattleUnit::DrawActionAt(BattleServices& services, i16 x, i16 y, ActState& act)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).DrawActionAtAdvance(services, x, y, act, scene);
    scene.Show(services);
}

// swd3e.exe 0x00499760: one frame of the unit walking back from the target to its place (the attack act, or `id` /
// `action` when given; the stand pose in the recoil act gives its x); 1 when there is no walk (a party role without
// kWalksHome or UnkFlag8 / UnkFlag20, an enemy without UnkFlag10, a forced unit) or it arrived (the acts and the line
// cleared, kWalksHome cleared). The walk takes 0x23 line steps a frame more each frame; with UnkFlag20 none until the
// act follows up, then a twelfth of the distance a frame more. Drawn over its shadow (an invisible unit smears) with
// the act's overlay. The rebuild's continuous motion splits it into HomeAdvance and the draw (DrawWalk).
int BattleUnit::ReturnHome(BattleServices& services, const BattleUnit& target, i32 id, i32 action)
{
    smooth::Scene scene;
    const int result = smooth::UnitFrame(*this).Home(services, target, id, action, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x00488CA0: an enemy's command taken from its object (skill skillA for choice 1, skillB for another): the
// command script and its value; a UnkFlag2 command gives its cast act, sets UnkCommandFlag8000 and command 2 (0);
// another sets unk2A8C to the choice and command 1 (1).
int BattleUnit::LoadEnemyCommand(Database& database, int choice, u32& act)
{
    this->command = UnitCommand{};
    this->unk29A4 = 0;
    const ObjectData& data = this->data->data;
    this->LoadCommand(database, choice == 1 ? data.skillA : data.skillB);
    this->unk29A4 = static_cast<u16>(this->command.value);

    if ((this->command.flags & UnitCommand::UnkFlag2) != 0)
    {
        act = this->command.castAct;
        this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
        this->SetCommand(BattleUnit::UnkCommand2);
        return 0;
    }

    this->unk2A8C = static_cast<u16>(choice);
    this->SetCommand(BattleUnit::UnkCommand1);
    return 1;
}

// swd3e.exe 0x00495A00: one frame of the unit fading out to invisible (mode 0) or back in (mode 1) at its place, over
// the shadow picture's shadow, glowLevel counting up to kInvisibleFadeFrames; 0 when done: mode 0 makes the unit
// Invisible, both clear InvisibleMark and UnkMark400 (mode 1 also glowLevel)
int BattleUnit::FadeInvisible(BattleServices& services, int mode)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).FadeInvisibleAdvance(services, mode, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x0049A0E0: the proficiency counters of the two worn accessories (their unk50) stepped by `amount` up to
// their unk44 in `list`, a missing one appended; 1 when either accessory counts. The search goes on from where the
// first accessory's ended (the original keeps its node), so the second may be appended again.
int BattleUnit::StepAccessoryCounters(CounterList& list, u16 amount)
{
    int counts = 0;
    CounterNode* at = list.id != 0 ? list.next.get() : &list;

    for (const ObjectRecord& accessory : this->accessories)
    {
        const ObjectData& data = accessory.data;
        const auto id = static_cast<i16>(data.act);

        if (id == 0)
        {
            continue;
        }

        counts = 1;

        while (at->id != id && at->next != nullptr)
        {
            at = at->next.get();
        }

        if (at->id == id)
        {
            if (at->done != 0)
            {
                continue;
            }

            at->value = static_cast<u16>(at->value + amount);

            if (at->value >= data.counterMax)
            {
                at->value = data.counterMax;
            }

            at->percent = static_cast<u16>(
                crt::Ftol(static_cast<double>(at->value) / static_cast<double>(data.counterMax) * kCounterPercent)
            );
            continue;
        }

        at->next = std::make_unique<CounterNode>();
        at = at->next.get();
        at->id = id;
        at->value = amount;
        at->percent = static_cast<u16>(
            crt::Ftol(static_cast<double>(amount) / static_cast<double>(data.counterMax) * kCounterPercent)
        );
        ++list.id;
    }

    return counts;
}

// swd3e.exe 0x0047E720: one tick of the action gauge: nothing while held, stopped (Unk4000), defeated or full
// (unk2AB0). Below gaugeFull it fills by a quarter of the agility (an eighth while Slowed; a quarter more and 1 with
// `boost`), 30% more with SpeedUp, 30% less with SpeedDown, and less by half the proficiency percent of it and 4 while
// Invisible (0). Full, it is marked full, the main act's effect cleared, the cast act marked waiting and the facing
// of a unit with facingTurnsBack turned back (1).
int BattleUnit::StepGauge(BattleServices& services, int boost)
{
    if ((this->marks & BattleUnit::Held) != 0 || (this->marks & BattleUnit::Unk4000) != 0)
    {
        return 0;
    }

    if (this->defeated == 1 || this->unk2AB0 == 1)
    {
        return 0;
    }

    const u16 gauge = this->gauge;

    if (static_cast<i32>(gauge) >= services.state.gaugeFull)
    {
        this->unk2AB0 = 1;
        this->unk2AEC = 0;

        if (this->facingTurnsBack == 1)
        {
            this->facingTurnsBack = 0;

            if (this->guestControl == 0)
            {
                this->facing = 0;
            }
        }

        this->mainAct.effectA = 0;
        this->mainAct.effectQ = 0;
        this->castAct.waiting = 1;
        return 1;
    }

    const u32 status = this->status;
    int speed = static_cast<u16>(this->battleRecord->agility) >> kAgilityShare;

    if ((status & BattleUnit::Slowed) != 0)
    {
        speed >>= 1;
    }

    if (boost == 1)
    {
        speed += speed / kBoostShare + 1;
    }

    int extra = 0;
    int cut = 0;

    if ((status & BattleUnit::SpeedUp) != 0)
    {
        extra = kSpeedChangePercent * speed / kPercent;
    }

    if ((status & BattleUnit::SpeedDown) != 0)
    {
        cut = kSpeedChangePercent * speed / kPercent;
    }

    if ((status & BattleUnit::Invisible) != 0)
    {
        cut += this->proficiency * speed / kPercent / 2 + kInvisibleSlowdown;
    }

    this->unk2AB0 = 0;
    this->gauge = static_cast<u16>(gauge - cut + extra + speed);
    return 0;
}

// swd3e.exe 0x004956B0: one frame of the unit's main act and overlay fading out (tinted by glowLevel, falling by 2 a
// frame while not held); 1 once at kFadedOut (the recoil act cleared)
int BattleUnit::FadeMainOut(BattleServices& services)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).FadeMainOutAdvance(services, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x00495870: one frame of the unit's main act and overlay brightening (tinted by brightLevel, rising by 1
// a frame while not held); 1 once at 0 (UnkMark200 cleared, brightLevel 1)
int BattleUnit::FadeMainIn(BattleServices& services)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).FadeMainInAdvance(services, scene);
    scene.Show(services);
    return result;
}

// The draw of FadeMainOut and FadeMainIn (inline in both): the main act's picture and its overlay additively at the
// unit's place, tinted by `tint`.
void BattleUnit::DrawMainFaded(BattleServices& services, int tint)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).DrawMainFadedAdvance(services, tint, scene);
    scene.Show(services);
}

// swd3e.exe 0x00497790: the unit's defence: a party role's from its battle record (+30% DefenseUp, -30% DefenseDown,
// +70% Barrier), an enemy's from its object (no barrier)
int BattleUnit::Defense() const
{
    const u32 status = this->status;
    const int base = this->kind == 1 ? this->battleRecord->defenseBonus : this->data->data.defense;
    int up = 0;
    int down = 0;
    int barrier = 0;

    if ((status & BattleUnit::DefenseUp) != 0)
    {
        up = kDefenseChangePercent * base / kPercent;
    }

    if ((status & BattleUnit::DefenseDown) != 0)
    {
        down = kDefenseChangePercent * base / kPercent;
    }

    if (this->kind == 1 && (status & BattleUnit::Barrier) != 0)
    {
        barrier = kBarrierPercent * base / kPercent;
    }

    return up + barrier + base - down;
}

// swd32.exe 0x00479850 (unmatched in swd3e): one frame of the unit's defeat while `dying`; 1 once it is over (the cast
// act cleared, the
// turn reset, the unit defeated). A party role or a unit with unk2AF8 (not forced) ends at once: a role is marked
// Defeated in its record and its effects expire. Others play the defeat pose (the fixed pose, or kParticlePose for
// an UnkKind20 object) on the picture of the cast act's action kDefeatAction, glowLevel counting the frames.
int BattleUnit::DefeatFrame(BattleServices& services)
{
    if (this->dying == 0)
    {
        return 0;
    }

    this->unk2AAC = 1;
    this->defeated = 1;
    ActState& act = this->castAct;
    act.id = this->actId;
    act.action = kDefeatAction;

    if ((this->kind == 1 || this->unk2AF8 == 1) && this->autoControl == 0 && this->guestControl == 0)
    {
        this->glowLevel = kDefeatedAtOnce;
        this->defeatPose = 0;

        if (this->kind == 1)
        {
            this->roleRecord->condition = static_cast<u16>(this->roleRecord->condition | RoleRecord::Defeated);
        }

        this->mainAct.effectA = 0;
        this->mainAct.effectQ = 0;
        this->glowLevel = 0;
        this->gauge = 0;
        act = ActState{};
        this->ResetTurn(services);
        this->unk2AAC = 0;
        this->dying = 0;
        this->defeated = 1;
        this->ExpireEffects(services, 1);
        return 1;
    }

    if (services.acts.Run(act) == 0)
    {
        return 1;
    }

    this->shadowPicture = services.archive.GetPicture(act.frame, 0);
    int offsetX = act.x;

    if (this->facingTurnsBack == 1)
    {
        this->facing = 0;
    }

    // A missing picture is read through a null pointer in the original.
    if (this->shadowPicture == nullptr)
    {
        return 1;
    }

    const Picture& picture = *this->shadowPicture;

    if (this->facing == 1 && act.x != 0)
    {
        offsetX = picture.width - act.x;
    }

    this->drawFlags &= Painter::RleSource | Painter::Mirror | Painter::Flip;

    if ((this->data->data.kind & ObjectData::UnkKind20) != 0)
    {
        this->defeatPose = kParticlePose;
    }

    if (this->fixedPose != 0)
    {
        this->defeatPose = this->fixedPose;
    }

    const int x = this->placement.x - offsetX;
    const int y = this->placement.y - act.y;

    switch (this->defeatPose)
    {
        case kRisingPose:
            return this->DefeatRising(services, picture, x, y);

        case kBurstPose:
        case kSweepPose:
            return this->DefeatBurst(services, picture, x, y);

        case kSplitRowsPose:
            return this->DefeatSplitRows(services, picture, x, y);

        case kSplitColumnsPose:
            return this->DefeatSplitColumns(services, picture, x, y);

        case kFillPose:
        case kBlueFillPose:
            return this->DefeatFill(services, picture, x, y);

        case kFadePose:
            return this->DefeatFadeOut(services, picture);

        case kQuartersPose:
            return this->DefeatQuarters(services, picture, x, y);

        case kWavePose:
            return this->DefeatWave(services, picture);

        case kSinkPose:
            return this->DefeatSink(services, picture);

        case kZoomPose:
            return this->DefeatZoom(services, picture);

        case kStripsPose:
            return this->DefeatStrips(services, picture, x, y);

        case kHalvesPose:
            return this->DefeatHalves(services, picture, x, y);

        case kSlidePose:
            return this->DefeatSlide(services, picture);

        case kHopPose:
            return this->DefeatHop(services, picture, x, y);

        case kProjectedPose:
            return this->DefeatProjected(services, picture);

        case kParticlePose:
            return this->DefeatParticles(services, picture, x, y);

        default:
            return 0;
    }
}

// The end of a defeat pose (inline in DefeatFrame): the gauge and the cast act cleared, the turn reset, the unit
// defeated.
int BattleUnit::EndDefeat(BattleServices& services)
{
    this->gauge = 0;
    this->castAct = ActState{};
    this->ResetTurn(services);
    this->unk2AAC = 0;
    this->defeated = 1;
    this->dying = 0;
    return 1;
}

// The end of a particle pose: the decoded pixels freed and the effect cleared as well.
int BattleUnit::EndParticleDefeat(BattleServices& services)
{
    this->glowLevel = 0;
    this->absorbPixels.clear();
    this->absorb = ParticleEffect{};
    return this->EndDefeat(services);
}

// The defeat sound at the first frame of a pose, then the picture as the source.
void BattleUnit::StartDefeatDraw(BattleServices& services, const Picture& picture)
{
    if (this->glowLevel == 0)
    {
        services.audio.PlaySound(kDefeatSound, services.config.soundVolume);
    }

    services.painter.SetSource(picture.data.data());
}

// One of the split halves of a defeat pose drawn additively within (left, top, right, bottom), tinted by -glowLevel.
void BattleUnit::DrawDefeatPiece(
    BattleServices& services,
    const Picture& picture,
    int left,
    int top,
    int right,
    int bottom,
    int x,
    int y,
    int height
)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).DrawDefeatPieceAdvance(services, picture, left, top, right, bottom, x, y, height, scene);
    scene.Show(services);
}

// Pose 1: the picture rises by glowLevel (0x1F more a frame up to 0x1E0), glowing, scaled up by glowLevel rows.
int BattleUnit::DefeatRising(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatRisingAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Poses 2 and 8: the picture breaks into particles, into its middle (2, with the cast act's sound) or off the side
// the unit faces (8); 1 once they are gone.
int BattleUnit::DefeatBurst(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatBurstAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// The explosion of a particle pose; glowLevel kParticlesDone once it is over.
int BattleUnit::RunDefeatParticles(BattleServices& services)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).RunDefeatParticlesAdvance(services, scene);
    scene.Show(services);
    return result;
}

// The picture decoded into absorbPixels for a particle pose.
void BattleUnit::DecodeForParticles(BattleServices& services, const Picture& picture)
{
    this->absorbPixels.clear();
    const std::optional<RawPicture> raw = RleCodec(services.painter).Decode(picture.data);

    if (raw.has_value())
    {
        this->absorbPixels.resize(raw->pixels.size() / sizeof(u16));
        std::memcpy(this->absorbPixels.data(), raw->pixels.data(), this->absorbPixels.size() * sizeof(u16));
    }

    this->absorb.pixels = this->absorbPixels;
}

// Pose 3: the left half rises and the right half sinks by 2 a frame, darkening, up to 0x20.
int BattleUnit::DefeatSplitRows(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatSplitRowsAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Pose 4: the top half slides left and the bottom half right by 2 a frame, darkening, up to 0x20.
int BattleUnit::DefeatSplitColumns(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatSplitColumnsAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Poses 5 and 10: a tint fills the picture from the top (5: dark, 0xE rows a frame; 10: blue, 4 rows a frame); once
// full the unit goes on with pose 2.
int BattleUnit::DefeatFill(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatFillAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Pose 6: the picture darkens by one tint step a frame at the unit's place down to -0x20.
int BattleUnit::DefeatFadeOut(BattleServices& services, const Picture& picture)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatFadeOutAdvance(services, picture, scene);
    scene.Show(services);
    return result;
}

// Pose 7: the four quarters fly apart diagonally by 1 a frame, darkening, up to 0x20 (the top left one drawn half
// high, the bottom right one 0x10 lower: as in the original).
int BattleUnit::DefeatQuarters(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatQuartersAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Pose 9: the picture waves away (wave row glowLevel / 3 + 1), more transparent each frame, up to 0x2D frames.
int BattleUnit::DefeatWave(BattleServices& services, const Picture& picture)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatWaveAdvance(services, picture, scene);
    scene.Show(services);
    return result;
}

// Pose 11: the picture sinks by 10 a frame, darkening, under its fading translucent copy, down to -0x20.
int BattleUnit::DefeatSink(BattleServices& services, const Picture& picture)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatSinkAdvance(services, picture, scene);
    scene.Show(services);
    return result;
}

// Pose 12: the picture zooms about the main act's position, wider and flatter each frame, for 0x1E frames.
int BattleUnit::DefeatZoom(BattleServices& services, const Picture& picture)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatZoomAdvance(services, picture, scene);
    scene.Show(services);
    return result;
}

// Pose 13: four strips of a quarter of the height slide apart, alternately left and right, by 2 a frame, darkening.
int BattleUnit::DefeatStrips(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatStripsAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Pose 14: the halves slide apart like pose 4, fast (4 a frame) to 8, then waiting 8 frames, then 1 a frame to 0x20.
int BattleUnit::DefeatHalves(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatHalvesAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Pose 15: the picture slides right by 2 a frame, darkening, beside its fading translucent copy, down to -0x20.
int BattleUnit::DefeatSlide(BattleServices& services, const Picture& picture)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatSlideAdvance(services, picture, scene);
    scene.Show(services);
    return result;
}

// Pose 50: the picture hops (BattleUnit::HopHeights) upside down, more transparent each frame, for 15 frames; the
// cast act's sound plays first (the stand act's sound set to kDefeatSound).
int BattleUnit::DefeatHop(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatHopAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// Pose 51: the picture projected (DrawScaled) at the unit's place, growing by 4 in both scales and turned, every
// other frame number up to 100; the cast act's sound plays first.
int BattleUnit::DefeatProjected(BattleServices& services, const Picture& picture)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatProjectedAdvance(services, picture, scene);
    scene.Show(services);
    return result;
}

// Pose 100: the unit wobbles (recoilTint, flickering up by 4 and down by 1, kWobbleSound at its top) six times, then
// shakes while it breaks into particles that fly off a strip across the field.
int BattleUnit::DefeatParticles(BattleServices& services, const Picture& picture, int x, int y)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DefeatParticlesAdvance(services, picture, x, y, scene);
    scene.Show(services);
    return result;
}

// The place of WalkToTarget (part of 0x00482310): the act's offset and the stand pose's mirrored by the facing, the
// line from the unit's place to the target's, kWalkSteps line steps a frame more each frame (with UnkFlag20 none until
// the act follows up, then a sixteenth of the distance a frame more), arrived once its x is reached.
BattleUnit::Stride BattleUnit::WalkStride(
    BattleServices& services,
    const BattleUnit& target,
    const ActState& act,
    const ActState& pose,
    i32 steps
) const
{
    Stride stride;
    stride.picture = services.archive.GetPicture(act.frame, act.picture);
    stride.steps = steps;

    if (stride.picture == nullptr)
    {
        return stride;
    }

    u16 targetX = 0;
    u16 targetY = 0;
    target.GetDrawPosition(targetX, targetY);
    stride.offsetX = static_cast<u16>(act.x);
    stride.flags = act.flags;
    stride.poseX = pose.x;

    if (this->facing == 1)
    {
        stride.flags ^= Painter::Mirror;
        stride.offsetX = static_cast<u16>(stride.picture->width - static_cast<u16>(act.x));
        stride.poseX = stride.picture->width - pose.x;
    }

    LineWalker& line = stride.line;
    line.fromX = this->placement.x - static_cast<i16>(stride.offsetX);
    line.fromY = this->placement.y - act.y;
    line.toX = targetX - stride.poseX;
    line.toY = targetY - act.y;
    int perFrame = kWalkSteps;

    if ((this->command.flags & UnitCommand::UnkFlag20) != 0)
    {
        perFrame = 0;

        if ((act.target & ActState::FollowsUp) != 0)
        {
            perFrame = (line.toX >= line.fromX ? line.toX - line.fromX : line.fromX - line.toX) / kWalkShare;
        }
        else
        {
            --stride.steps;
        }
    }

    for (int taken = 0; taken < perFrame * stride.steps; ++taken)
    {
        LineStep(line);

        if (line.fromX + line.x == line.toX)
        {
            stride.arrived = true;
            break;
        }
    }

    stride.x = stride.arrived ? line.toX : line.fromX + line.x;
    stride.y = stride.arrived ? line.toY : line.fromY + line.y;
    return stride;
}

// The place of ReturnHome (part of 0x00482840): the stand pose's offset (the act's when mirrored) and the pose's, the
// line from the target back to the unit's place, kHomeSteps line steps a frame more each frame (with UnkFlag20 none
// until the act follows up, then a twelfth of the distance a frame more), arrived once its x is reached.
BattleUnit::Stride BattleUnit::HomeStride(
    BattleServices& services,
    const BattleUnit& target,
    const ActState& act,
    const ActState& pose,
    i32 steps
) const
{
    Stride stride;
    stride.picture = services.archive.GetPicture(act.frame, act.picture);
    stride.steps = steps;

    if (stride.picture == nullptr)
    {
        return stride;
    }

    stride.offsetX = static_cast<u16>(pose.x);
    stride.flags = act.flags;
    stride.poseX = pose.x;

    if (this->facing == 1)
    {
        stride.flags ^= Painter::Mirror;
        stride.offsetX = static_cast<u16>(stride.picture->width - static_cast<u16>(act.x));
        stride.poseX = stride.picture->width - pose.x;
    }

    u16 targetX = 0;
    u16 targetY = 0;
    target.GetDrawPosition(targetX, targetY);
    LineWalker& line = stride.line;
    line.fromX = static_cast<i16>(targetX) - stride.poseX;
    line.fromY = static_cast<i16>(targetY) - act.y;
    line.toX = this->placement.x - static_cast<i16>(this->originX);
    line.toY = this->placement.y - act.y;
    int perFrame = kHomeSteps;

    if ((this->command.flags & UnitCommand::UnkFlag20) != 0)
    {
        perFrame = 0;

        if ((act.target & ActState::FollowsUp) != 0)
        {
            perFrame = (line.toX >= line.fromX ? line.toX - line.fromX : line.fromX - line.toX) / kHomeShare;
        }
        else
        {
            --stride.steps;
        }
    }

    for (int taken = 0; taken < stride.steps * perFrame; ++taken)
    {
        LineStep(line);

        if (line.fromX + line.x == line.toX)
        {
            stride.arrived = true;
            break;
        }
    }

    stride.x = stride.arrived ? line.toX : line.fromX + line.x;
    stride.y = stride.arrived ? line.toY : line.fromY + line.y;
    return stride;
}

// The place of a queued act (part of 0x0047FC40): its line less the act's offset (mirrored by the facing; a line end
// at y 0 stays at 0); while it shakes its y comes from ShakeOffsets and the shake frames count down, then it flies
// (one step more a frame, doubling while speeding) and arrives at the line's end. A shaking act keeps `taken`.
BattleUnit::Stride BattleUnit::FlightStride(
    BattleServices& services,
    const QueuedAction& node,
    const ActState& act,
    int taken
) const
{
    Stride stride;
    stride.picture = services.archive.GetPicture(act.frame, act.picture);
    stride.shakeFrames = node.shakeFrames;
    stride.flown = node.flown;
    stride.taken = taken;

    if (stride.picture == nullptr)
    {
        return stride;
    }

    stride.flags = act.flags;
    auto offsetX = static_cast<i16>(act.x);

    if (this->facing == 1)
    {
        stride.flags ^= Painter::Mirror;
        offsetX = static_cast<i16>(stride.picture->width - static_cast<u16>(act.x));
    }

    stride.offsetX = static_cast<u16>(offsetX);
    LineWalker& line = stride.line;
    line.fromX = node.fromX - offsetX;
    line.fromY = node.fromY - act.y;
    line.toX = node.toX - offsetX;
    line.toY = node.toY - act.y;

    if (node.fromY == 0)
    {
        line.fromY = 0;
    }

    if (node.toY == 0)
    {
        line.toY = 0;
    }

    if (stride.shakeFrames != 0)
    {
        // A negative phase (shake frames past 0x7FFF) is read before the table in the original.
        const int phase = static_cast<i16>(stride.shakeFrames) % static_cast<int>(BattleUnit::ShakeOffsets.size());
        line.y = phase >= 0 ? BattleUnit::ShakeOffsets[static_cast<std::size_t>(phase)] : 0;
        --stride.shakeFrames;
    }
    else
    {
        if (node.steps == 1)
        {
            if (stride.flown == 0)
            {
                stride.flown = 1;
            }

            stride.flown =
                static_cast<i16>(stride.flown <= kFlightDoubling ? 2 * stride.flown : stride.flown + kFlightGrowth);
        }
        else
        {
            ++stride.flown;
        }

        stride.taken = 0;

        while (stride.taken < stride.flown * node.steps)
        {
            LineStep(line);

            if (line.fromX + line.x == line.toX && line.fromY + line.y == line.toY)
            {
                stride.taken = kArrived;
                break;
            }

            ++stride.taken;
        }
    }

    stride.x = line.fromX + line.x;
    stride.y = line.fromY + line.y;
    stride.arrived = stride.taken == kArrived;
    return stride;
}

// swd3e.exe 0x00499060: one frame of the unit walking from its place to the target (the attack act's walk action, the
// stand pose in the recoil act giving its x): kWalkSteps line steps a frame more each frame; with UnkFlag20 none
// until the act follows up, then a sixteenth of the distance a frame more. A forced fading command (FadesIn) takes its
// command B at once; an invisible unit first fades out. Drawn over its shadow (an invisible unit smears; at the end it
// is drawn actOffsetX further left, as in the original). At the target the acts and the line are cleared and command A
// becomes command B. The rebuild's continuous motion splits it into WalkAdvance and the draw (DrawWalk).
void BattleUnit::WalkToTarget(BattleServices& services, const BattleUnit& target)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).Walk(services, target, scene);
    scene.Show(services);
}

// swd3e.exe 0x0049A000: whether the accessory command of `kind` can be taken: not when it costs more stamina than the
// role has; Unk1C not while Invisible, Unk1D not with its bit, Unk20 not under a Barrier, SpiritFan only with the
// equipment; Unk19 sets its unk26CC bit.
int BattleUnit::CanUseAccessory(u16 kind)
{
    if (static_cast<i32>(this->AccessoryValue(kind)) > this->roleRecord->stamina)
    {
        return 0;
    }

    switch (kind)
    {
        case kAccessory19:
            this->unk26CC |= kAccessory19Bit;
            return 1;

        case kAccessory1C:
            return (this->status & BattleUnit::Invisible) != 0 ? 0 : 1;

        case kAccessory1D:
            return (this->unk26CC & kAccessory1DBit) != 0 ? 0 : 1;

        case kAccessory20:
            return (this->status & BattleUnit::Barrier) != 0 ? 0 : 1;

        case kSpiritFanKind:
            return (this->equipmentFlags & BattleUnit::SpiritFan) != 0 ? 1 : 0;

        default:
            return 1;
    }
}

// swd3e.exe 0x0049B1B0: drain effect `slot` started: the source unit's idle picture breaks into particles flying into
// this unit (a point 0x32 left of and 0x28 below its act's anchor), bursting in half the picture's height
void BattleUnit::StartDrainEffect(BattleServices& services, int slot, BattleUnit& source)
{
    u16 sourceX = 0;
    u16 sourceY = 0;
    this->absorbSource = source.StepIdleAction(services);
    source.GetOrigin(sourceX, sourceY);
    ParticleEffect& effect = this->drains[static_cast<std::size_t>(slot)];
    std::vector<u16>& pixels = this->drainPixels[static_cast<std::size_t>(slot)];
    effect = ParticleEffect{};
    pixels.clear();

    // A source without its picture is read through a null pointer in the original.
    if (this->absorbSource == nullptr)
    {
        return;
    }

    const std::optional<RawPicture> raw = RleCodec(services.painter).Decode(this->absorbSource->data);

    if (raw.has_value())
    {
        pixels.resize(raw->pixels.size() / sizeof(u16));
        std::memcpy(pixels.data(), raw->pixels.data(), pixels.size() * sizeof(u16));
    }

    effect.pixels = pixels;
    effect.width = this->absorbSource->width;
    effect.height = this->absorbSource->height;
    effect.x = static_cast<i16>(sourceX) - 1;
    effect.y = static_cast<i16>(sourceY);
    effect.targetLeft =
        static_cast<i16>(this->actAnchorX) - static_cast<i16>(this->originX) + this->placement.x - kDrainLeft;
    effect.targetTop = this->placement.y - this->mainAct.y + kDrainDown;
    effect.targetWidth = 1;
    effect.targetHeight = 1;
    effect.speed = kDrainSpeed;
    effect.lifeBase = kDrainLife;
    effect.bursts = static_cast<u16>(this->absorbSource->height >> 1);
    effect.divisor = kDrainDivisor;
    effect.tintRed = kDrainTint;
    effect.tintGreen = kDrainTint;
    effect.tintBlue = kDrainTint;
    effect.flags = kStripFlags;

    if (source.Mirrored() == 1)
    {
        effect.flags = static_cast<u16>(effect.flags | ParticleEffect::Mirrored);
    }

    this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag800);
    services.state.projector.SetScreen(Display::Width, Display::Height);
}

// swd3e.exe 0x0049B330: one frame of drain effect `slot` (absorbing set); 1 once it is gone (its pixels freed and the
// effect cleared)
int BattleUnit::DrainEffectFrame(BattleServices& services, int slot)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).DrainEffectFrameAdvance(services, slot, scene);
    scene.Show(services);
    return result;
}

// swd32.exe 0x00476330 (unmatched in swd3e): whether `level` catches the enemy (never with UnkMark800 or when its
// object's unk91 is 0; a
// 0..11 roll is drawn and not used): more than 10 levels above its level 8 in 10, 6..10 above 5 in 10, exactly 5
// above unk91 in 10, otherwise never
int BattleUnit::CatchRoll(BattleServices& services, u8 level)
{
    if ((this->marks & BattleUnit::UnkMark800) != 0)
    {
        return 0;
    }

    services.random.Below(kUnusedCatchRoll);
    const ObjectData& data = this->data->data;

    if (data.catchChance == 0)
    {
        return 0;
    }

    const int above = level - data.level;

    if (above > kFarAbove)
    {
        return static_cast<u16>(services.random.Below(kCatchRoll)) < kFarCatch ? 1 : 0;
    }

    if (above > kNearAbove)
    {
        return static_cast<u16>(services.random.Below(kCatchRoll)) < kNearCatch ? 1 : 0;
    }

    return above >= kNearAbove && static_cast<u16>(services.random.Below(kCatchRoll)) < data.catchChance ? 1 : 0;
}

// swd3e.exe 0x0048F390: one frame of the unit staggering from a hit (hurtStep 0..5, one step every second frame not
// held); 1 once over (hurtStep back to 1, the attack act and the targets cleared, `charging` set) or when its act
// ends. A party role takes its hurt pose (its guard pose while guarding or marked, not held; a poisoned one the
// hurt pose), others the shadow action. The picture shakes back by kStaggerShake (UnkStatus4 copies the main act and
// stays), over its shadow, with a flash; an enemy also leaves a red flash behind, lifted by kStaggerRise (up with
// unk2B10, else to the side). Its effects, overlay and a recoil effect follow.
int BattleUnit::HurtFrame(BattleServices& services)
{
    smooth::Scene scene;
    const int result = smooth::UnitFrame(*this).Hurt(services, scene);
    scene.Show(services);
    return result;
}

// swd32.exe 0x00478B60 (unmatched in swd3e): one frame of the unit at rest (`paused`: its acts hold). drawAtSaved moves
// it first (1 shakes
// it a pixel at random, 2 lifts it 10 a frame to the top, 3 drops it 0x1E a frame back to its saved place, marking
// flag 26B8). A staggering unit (flag 26B8) runs HurtFrame. Otherwise, unless busy, the main act takes its pose: the
// stand pose (an enemy's plus unk2684; a party role's weak pose at a quarter of its hit points or less), the pose of
// a chosen command, a guard pose by guardPose, the defeated pose of a role, or unk2A0E. It is drawn over its shadow
// (mirrored by the facing; a forced draw adds Additive), with its outlines, its blend or wave, the barrier flash, a
// grey copy (UnkMark4), a red copy for command 2, the gold over a gold guard, its sound; command 6 of a
// role shows the thrown act; its overlay, idle effect, follow-up and effects follow. A fading or invisible unit runs
// its fade instead. The rebuild's continuous motion splits the frame into what it decides (IdleAdvance) and what it
// draws of the unit (IdleDraw), run here in the original's order.
void BattleUnit::IdleFrame(BattleServices& services, int paused)
{
    smooth::Scene scene;
    smooth::UnitFrame(*this).Idle(services, paused, scene);
    scene.Show(services);
}

// The movement of IdleFrame by drawAtSaved (inline in the original): 1 shakes the unit a pixel at random while nudge
// lasts (towards its side), 2 lifts it, 3 drops it back to its saved place.
void BattleUnit::MoveIdle(BattleServices& services)
{
    if (this->nudge == 0)
    {
        return;
    }

    if (this->drawAtSaved == kShaking)
    {
        if (static_cast<u16>(services.random.Below(kShakeChance)) == 1)
        {
            const bool back = this->kind == 1 ? this->facing != 0 : this->facing == 0;
            this->placement.x = static_cast<i16>(this->placement.x + (back ? -1 : 1));
        }

        --this->nudge;
        return;
    }

    if (this->drawAtSaved == kRising)
    {
        this->placement.y = static_cast<i16>(this->placement.y - kRiseStep);

        if (this->placement.y <= 0)
        {
            this->placement.y = 0;
            this->nudge = 0;
            this->drawAtSaved = 0;
        }

        return;
    }

    if (this->drawAtSaved == kDropping)
    {
        this->placement.y = static_cast<i16>(this->placement.y + kDropStep);
        const i16 home = this->savedPlacement.y;

        if (this->placement.y >= home)
        {
            this->placement.y = home;
            this->nudge = 0;
            this->drawAtSaved = 0;
            this->unk2B10 = 1;
            this->RaiseFlag26B8(services.state);
        }
    }
}

// swd3e.exe 0x00496650: an enemy chooses its command for the turn; `act` takes the act of an object or a cast; 1 when
// the command is chosen with its act (UnkCommandFlag8000 and command 2, or 0xF for UnkAction1000), 0 for an attack
// or a skill of command 1, 1 at once while UnkCommandFlag8000 is set. A heal (healChoice) comes first; else objects
// and skills take turns (choicePhase), and a status of bit 5 or 8 or no idle turn yet fall back to the attack after an
// unused roll. An object or skill is tried with a roll of 0..9 below idleTurns plus its chance; the third object
// comes every 7 turns, the second skill every 5. The attack is the object's skill script with its attack added.
int BattleUnit::ChooseCommand(BattleServices& services, Database& database, u32& act)
{
    Random& random = services.random;
    ObjectData& data = this->data->data;
    int choice = kNoChoice;
    int chosen = 0;

    if ((this->commandFlags & BattleUnit::UnkCommandFlag8000) != 0)
    {
        return 1;
    }

    this->command = UnitCommand{};

    if (this->healChoice != 0)
    {
        choice = kHealChoice;
    }
    else if (data.itemACount != 0)
    {
        if (data.skillACount != 0 && this->choicePhase == kItemsChosen)
        {
            choice = kSkillChoice;
            this->choicePhase = kSkillsChosen;
        }
        else
        {
            choice = kItemChoice;
            this->choicePhase = kItemsChosen;
        }
    }
    else if (data.itemBCount != 0 || data.itemCCount != 0)
    {
        choice = kItemChoice;
        this->choicePhase = kItemsChosen;
    }
    else if (data.skillACount != 0 || data.skillBCount != 0)
    {
        choice = kSkillChoice;
        this->choicePhase = kSkillsChosen;
    }

    bool rollIdle = true;

    if (this->StatusBit5() == 1)
    {
        choice = kStatusChoice;
    }
    else if (choice == kHealChoice)
    {
        rollIdle = false;
    }

    if (rollIdle && this->idleTurns == 0)
    {
        random.Below(kUnusedChoiceRoll);
        choice = kIdleChoice;
        this->choicePhase = static_cast<u8>(this->choicePhase != kItemsChosen ? 1 : 0);
    }

    // A command the unit may not use: a forced unit's skill with kNoForcedStatus, or an UnkAction1000 one while
    // unk53BF28 is set.
    auto barred = [this, &services]
    {
        return (this->autoControl == 1 && (static_cast<u32>(this->command.unk08) & kNoForcedStatus) != 0) ||
               ((this->command.actionFlags & UnitCommand::UnkAction1000) != 0 && services.state.unk53BF28 != 0);
    };

    if (choice == kItemChoice)
    {
        if ((this->marks & BattleUnit::UnkMark100) != 0 || data.itemChance == 0)
        {
            return this->ChooseAttack(database);
        }

        const u32 value = random.Below(kChoiceRoll);
        int item = static_cast<int>(value);

        if (value <= kFirstItemRoll)
        {
            item = 0;
        }
        else if (value <= kSecondItemRoll)
        {
            item = 1;
        }
        else if (value <= kChoiceRoll)
        {
            item = 2;
        }

        if (this->itemWait >= kItemWaitTurns)
        {
            if (data.itemC != 0 && data.itemCCount != 0)
            {
                item = 2;
            }

            this->itemWait = 0;
        }

        // An object used: its skill script is the command (inline in the original).
        auto useItem = [&](u16 id, u16& count, i32 readyAction)
        {
            if (id == 0 || count == 0)
            {
                return false;
            }

            if (random.Below(kChoiceRoll) >= static_cast<u32>(this->idleTurns + data.itemChance))
            {
                return false;
            }

            database.LoadObject(this->object, id);
            this->LoadCommand(database, this->object.data.skillScript);

            if (barred())
            {
                return false;
            }

            act = this->object.data.act;
            --count;

            if (this->actId == kReadyAct)
            {
                this->readyAction = readyAction;
            }

            return true;
        };

        if (item == 0)
        {
            chosen = useItem(data.itemA, data.itemACount, kFirstItemAction) ? 1 : 0;
        }
        else if (item == 1)
        {
            chosen = useItem(data.itemB, data.itemBCount, kSecondItemAction) ? 1 : 0;
        }
        else if (item == 2)
        {
            if (useItem(data.itemC, data.itemCCount, kThirdItemAction))
            {
                chosen = 1;
                this->itemWait = 0;
            }
        }

        Database::FreeDescription(this->object);
        std::fill_n(this->skillName.begin(), kClearedNameBytes, '\0');

        if (chosen != 1)
        {
            return this->ChooseAttack(database);
        }

        const std::span<const char> name(this->object.data.name);
        const std::size_t length = std::min<std::size_t>(strnlen(name.data(), name.size()), this->skillName.size() - 1);
        std::copy_n(name.begin(), length, this->skillName.begin());
        this->skillName[length] = '\0';
        this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
        this->unk29A4 = static_cast<u16>(this->unk29A4 + static_cast<u16>(this->command.value));
        const bool special = (this->command.actionFlags & UnitCommand::UnkAction1000) != 0;
        this->SetCommand(special ? BattleUnit::UnkCommand15 : BattleUnit::UnkCommand2);
        this->idleTurns = 0;
        return 1;
    }

    if (choice == kSkillChoice)
    {
        if (data.skillChance == 0)
        {
            return this->ChooseAttack(database);
        }

        const u32 value = random.Below(kChoiceRoll);
        int skill = static_cast<int>(value);

        if (value <= kFirstSkillRoll)
        {
            skill = 0;
        }
        else if (value <= kChoiceRoll)
        {
            skill = 1;
        }

        if (this->skillWait >= kSkillWaitTurns)
        {
            if (data.skillB != 0 && data.skillBCount != 0)
            {
                skill = 1;
            }

            this->skillWait = 0;
        }

        if (skill != 0 && skill != 1)
        {
            return this->ChooseAttack(database);
        }

        const u16 id = skill == 0 ? data.skillA : data.skillB;
        u16& count = skill == 0 ? data.skillACount : data.skillBCount;

        if (id == 0 || count == 0)
        {
            return this->ChooseAttack(database);
        }

        if (random.Below(kChoiceRoll) >= static_cast<u32>(this->idleTurns + data.skillChance))
        {
            return this->ChooseAttack(database);
        }

        this->LoadCommand(database, id);

        if (skill == 1)
        {
            this->skillWait = 0;
        }

        if (barred())
        {
            return this->ChooseAttack(database);
        }

        this->unk29A4 = static_cast<u16>(this->command.value);
        --count;
        this->idleTurns = 0;

        if ((this->command.actionFlags & UnitCommand::UnkAction1000) != 0)
        {
            this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
            this->SetCommand(BattleUnit::UnkCommand15);
            return 1;
        }

        if ((this->command.flags & UnitCommand::UnkFlag2) != 0)
        {
            act = this->command.castAct;
            this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
            this->SetCommand(BattleUnit::UnkCommand2);
            return 1;
        }

        this->unk2A8C = static_cast<u16>(skill + 1);
        this->SetCommand(BattleUnit::UnkCommand1);
        this->MarkDelayedAttack();
        return 0;
    }

    if (choice == kHealChoice && (this->marks & BattleUnit::UnkMark100) == 0 && data.itemChance != 0 &&
        (this->healChoice == kFirstHeal || this->healChoice == kSecondHeal))
    {
        const bool first = this->healChoice == kFirstHeal;
        database.LoadObject(this->object, first ? data.healItemA : data.healItemB);
        Database::FreeDescription(this->object);
        act = this->object.data.act;
        this->LoadCommand(database, this->object.data.skillScript);
        this->restoreHitPoints = this->object.data.hitPointsUp;
        this->healChoice = 0;
        --(first ? data.healCountA : data.healCountB);
        this->commandFlags = static_cast<u16>(this->commandFlags | BattleUnit::UnkCommandFlag8000);
        this->SetCommand(BattleUnit::UnkCommand2);

        if (this->actId == kReadyAct)
        {
            this->readyAction = kHealAction;
        }

        return 1;
    }

    return this->ChooseAttack(database);
}

// The attack of ChooseCommand (inline in the original): the object's skill script as command 1, every wait counted
// up, the attack (a party role's equipment bonus) added to the value; 0.
int BattleUnit::ChooseAttack(Database& database)
{
    this->command = UnitCommand{};
    this->LoadCommand(database, this->data->data.skill);
    this->SetCommand(BattleUnit::UnkCommand1);
    ++this->idleTurns;
    ++this->itemWait;
    ++this->skillWait;

    if (this->kind != 0)
    {
        this->unk29A4 = static_cast<u16>(this->unk29A4 + this->battleRecord->attackBonus);
    }
    else
    {
        this->unk29A4 = static_cast<u16>(this->unk29A4 + this->data->data.attack);
    }

    this->MarkDelayedAttack();
    return 0;
}

// The end of a command-1 choice (inline in the original): a command with UnkFlag10 chosen as command 1 becomes command
// 0xC8 with command B 1.
void BattleUnit::MarkDelayedAttack()
{
    if ((this->command.flags & UnitCommand::UnkFlag10) != 0 && this->commandA == BattleUnit::UnkCommand1)
    {
        this->commandB = BattleUnit::UnkCommand1;
        this->commandA = kDelayedCommand;
    }
}

// swd3e.exe 0x00498560: the damage of an enemy's hit on `target` (-1 shows no number). A barrier stops it; a held
// target is freed by a hit that heals nothing; the buff lands first. A party role takes the ailment unless its spirit
// fan guards against it by proficiency (an ailment with kSacrificeAilment defeats the attacker and gives the target's
// hit points), then its defence and record resistances count; an enemy target its object's. Command 1 rolls on the
// level, raised and cut by the attack buffs, less the defence (else the bare value, at least 1, rolled again unless
// unk2A8C); other commands add a roll to a positive value. The resistance of the command's element scales it, an
// effect of its element halves it, restoreHitPoints heals instead, a guard cuts it by 10..39%, and a party role gains
// fury. The original computes in 16 bits.
i16 BattleUnit::EnemyHitDamage(BattleServices& services, BattleUnit& target)
{
    Random& random = services.random;
    const ObjectData& data = this->data->data;
    i16 damage = static_cast<i16>(this->unk29A4);
    i32 raise = 0;
    i32 cut = 0;
    bool resisted = false;
    int defense = 0;
    std::array<u8, kResistances> resistances = {};
    target.PushTarget(BattleUnit::DamageTarget);

    if (target.HasEffectBits(BattleUnit::UnkStatus4) == 1)
    {
        return 0;
    }

    if (this->restoreHitPoints == 0 && target.IsHeld() == 1)
    {
        target.CureEffects(services, BattleUnit::UnkStatus8);
    }

    if (target.AddBuff(services, this->CommandSkill()) == 1)
    {
        return -1;
    }

    if (target.kind == BattleUnit::PartyRole)
    {
        const auto* record = static_cast<const RoleRecord*>(target.SelectEntry(1));
        defense = target.Defense();
        std::copy_n(record->resistances.begin(), resistances.size(), resistances.begin());
        const u16 ailment = this->command.ailment;

        if (ailment != 0 && !target.FanGuards(services, ailment))
        {
            if (target.AddAilment(services, this->CommandSkill()) == 1)
            {
                target.RaiseFlag26B8(services.state);

                if ((ailment & kSacrificeAilment) != 0)
                {
                    this->Damage(services, kSacrificeDamage, 0, 0);
                    i32 hitPoints = 0;
                    i32 maximum = 0;
                    target.GetHitPoints(hitPoints, maximum);
                    return static_cast<i16>(hitPoints);
                }
            }
            else if (this->command.value != 0 || this->unk29A4 != 0)
            {
                target.RaiseFlag26B8(services.state);
            }
            else
            {
                target.RollEvade(services, kEvadeRoll);
            }

            if (this->command.value == 0 && this->unk29A4 == 0)
            {
                return -1;
            }
        }
    }
    else
    {
        const ObjectData& targetData = target.Data()->data;
        defense = targetData.defense;
        std::copy_n(
            targetData.resistances.begin(),
            std::min(resistances.size(), targetData.resistances.size()),
            resistances.begin()
        );
        target.SetDefeatPose(LowByte(this->command.pose));
    }

    const u16 element = this->command.resistance;
    const bool elemental = element >= 1 && element <= kResistances;

    auto scaleByElement = [&]
    {
        if (elemental && ScaleByTenths(static_cast<i8>(resistances[element - 1U]), damage) == 1)
        {
            resisted = true;
        }
    };

    auto levelRoll = [&random, &data]
    {
        return static_cast<i32>(random.Below(
            static_cast<u16>(
                static_cast<i16>(data.level) / static_cast<int>(kLevelShare) + static_cast<int>(kLevelRollBase)
            )
        ));
    };

    if (this->commandA != BattleUnit::UnkCommand1)
    {
        const u32 spread = random.Below(kEnemySpreadRoll);
        const auto limit = static_cast<u16>((data.level + data.unk60) / static_cast<u16>(spread + kEnemySpreadBase));
        const i32 bonus = static_cast<i32>(random.Below(limit)) + 1;

        if (damage > 0)
        {
            damage = static_cast<i16>(damage + bonus);
        }

        if (this->commandA == BattleUnit::UnkCommand2)
        {
            scaleByElement();
        }
    }
    else
    {
        const auto attack = static_cast<i16>(levelRoll() + 1 + damage);

        if ((this->status & BattleUnit::AttackUp) != 0)
        {
            raise = kAttackBuffPercent * attack / kPercent;
        }

        if ((this->status & BattleUnit::AttackDown) != 0)
        {
            cut = kAttackBuffPercent * attack / kPercent;
        }

        const auto raised = static_cast<i16>(raise - cut + attack);

        if (raised > static_cast<i32>(static_cast<u16>(defense)))
        {
            damage = static_cast<i16>(raised - defense);
        }
        else
        {
            const auto value = static_cast<i16>(this->unk29A4);

            if ((this->status & BattleUnit::AttackUp) != 0)
            {
                raise = kAttackBuffPercent * value / kPercent;
            }

            if ((this->status & BattleUnit::AttackDown) != 0)
            {
                cut = kAttackBuffPercent * value / kPercent;
            }

            auto bare = static_cast<i16>(raise - defense - cut + value);

            if (bare < 0)
            {
                bare = 1;
            }

            damage = this->unk2A8C != 0 ? bare : static_cast<i16>(levelRoll() + 1 + bare);
        }

        scaleByElement();
    }

    if (target.HasEffectElement(static_cast<i16>(this->command.element)) == 1)
    {
        damage = static_cast<i16>(damage / 2);
    }

    if (this->restoreHitPoints != 0)
    {
        target.PushTarget(BattleUnit::HealTarget);
        damage = static_cast<i16>(-static_cast<i16>(this->restoreHitPoints));
    }

    if (resisted)
    {
        target.PushTarget(BattleUnit::HealTarget);
    }

    if (target.GetGuard() == 1)
    {
        const auto share = static_cast<u16>(random.Below(kGuardCutRoll) + kGuardCutBase);
        damage = static_cast<i16>(damage * share / -kPercent + damage);
    }

    if (target.kind == BattleUnit::PartyRole && damage > 0)
    {
        static_cast<BattleRole&>(target).AddFury(services, damage);
    }

    return damage;
}

// The spirit fan of EnemyHitDamage (inline in the original): a worn one counts its proficiency up by kFanStep and, by
// each kFanShare of it, guards against more ailments (all at full proficiency).
bool BattleUnit::FanGuards(BattleServices& services, u16 ailment)
{
    if (this->CanUseAccessory(kSpiritFanKind) != 1)
    {
        return false;
    }

    CounterList& proficiency = services.party.fame.lists[Fame::ProficiencyList];
    this->StepAccessoryCounters(proficiency, kFanStep);
    const auto level = static_cast<u16>(this->AccessoryProficiency(kSpiritFanKind, proficiency)) / kFanShare;

    if (level == kFanGuards.size())
    {
        return true;
    }

    return level < kFanGuards.size() && (ailment & kFanGuards[level]) != 0;
}

// swd3e.exe 0x00488F50: one frame of the unit running off the field, counting fadeLevel down; 1 once it is down to
// kRunEnd (the recoil act cleared) or the act ends. The recoil act runs the shadow action turned away from the
// facing, stepping kRunStep a frame; its first picture plays a step sound, fadeLevel kRunSoundAt the run sound.
int BattleUnit::RunAway(BattleServices& services)
{
    smooth::Scene scene;
    const auto result = smooth::UnitFrame(*this).RunAwayAdvance(services, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x0047FB90: the worn element orbs (AccessoryKind 0x33) step their proficiency counters (their unk50) in
// `list` by kOrbStep up to their unk44, when hit by `command` of an element 1..9 (the original also tests the address
// of the orb's resistance to it, never null); 1 when an orb counts. Each search starts at the head of the list, its
// count compared as an id; a missing counter is appended at the end and counted.
int BattleUnit::StepOrbCounters(CounterList& list, const UnitCommand& command)
{
    int counts = 0;

    for (const ObjectRecord& accessory : this->accessories)
    {
        const ObjectData& data = accessory.data;
        const auto id = static_cast<i16>(data.act);

        if (id == 0 || data.unk48 != static_cast<u16>(AccessoryKind::ElementOrb) || command.resistance < 1 ||
            command.resistance > kResistances)
        {
            continue;
        }

        counts = 1;
        CounterNode* at = &list;

        while (at->id != id && at->next != nullptr)
        {
            at = at->next.get();
        }

        if (at->id == id)
        {
            if (at->done != 0)
            {
                continue;
            }

            at->value = static_cast<u16>(at->value + kOrbStep);

            if (at->value >= data.counterMax)
            {
                at->value = data.counterMax;
            }

            at->percent = static_cast<u16>(
                crt::Ftol(static_cast<double>(at->value) / static_cast<double>(data.counterMax) * kCounterPercent)
            );
            continue;
        }

        at->next = std::make_unique<CounterNode>();
        at = at->next.get();
        at->id = id;
        at->value = kOrbStep;
        at->percent = static_cast<u16>(
            crt::Ftol(static_cast<double>(kOrbStep) / static_cast<double>(data.counterMax) * kCounterPercent)
        );
        ++list.id;
    }

    return counts;
}

// swd3e.exe 0x0048FE60: one frame of the unit's damage numbers (while unk2AD8): from damageSlot to targetCount, each
// slot waits out its delay, then its four digits (font pushedTargets) hop up one after another by DigitHops (not while
// held); once all have landed (step 9) a pending extra number (extraNumber: digits unk53AEDC..E4) becomes the next
// slot, and the slot rises and fades by 2 a frame up to kNumberFadeEnd before it is cleared and the next is shown. A
// leading zero digit is not drawn. 1 once every slot is done (all cleared).
int BattleUnit::DrawDamageNumbers(BattleServices& services)
{
    smooth::Scene scene;
    const int result = smooth::UnitFrame(*this).Numbers(services, scene);
    scene.Show(services);
    return result;
}

// swd3e.exe 0x00489180: object `choice` (0..2: itemA, itemB, itemC) of the enemy loaded into `object`, its name copied
// to `name` and `id` set to itemA whichever was chosen (as in the original); 0 without that object
int BattleUnit::ItemName(Database& database, int choice, std::string& name, u32& id)
{
    const ObjectData& data = this->data->data;
    u16 item = 0;

    if (choice == 0)
    {
        item = data.itemA;
    }
    else if (choice == 1)
    {
        item = data.itemB;
    }
    else if (choice == 2)
    {
        item = data.itemC;
    }

    if (item == 0)
    {
        return 0;
    }

    database.LoadObject(this->object, item);
    id = this->data->data.itemA;
    name = ObjectName(this->object);
    return 1;
}

// swd3e.exe 0x00489260: skill `choice` (0: skillA, 1: skillB) of the enemy loaded into `object` and its name copied
// to `name`; 0 without that skill
int BattleUnit::SkillName(Database& database, int choice, std::string& name)
{
    const ObjectData& data = this->data->data;
    u16 skill = 0;

    if (choice == 0)
    {
        skill = data.skillA;
    }
    else if (choice == 1)
    {
        skill = data.skillB;
    }

    if (skill == 0)
    {
        return 0;
    }

    database.LoadObject(this->object, skill);
    name = ObjectName(this->object);
    return 1;
}

// swd3e.exe 0x00488D40: the object an enemy drops (dropItem) on a roll of 0..19 below its dropChance; 0 otherwise
u16 BattleUnit::RollDrop(BattleServices& services)
{
    const ObjectData& data = this->data->data;

    if (data.dropItem == 0 || services.random.Below(kDropRoll) >= data.dropChance)
    {
        return 0;
    }

    return this->data->data.dropItem;
}

// swd3e.exe 0x0047FF60: the first worn accessory whose proficiency counter in `list` is full (at its unk44) and not
// yet done: the counter marked done and the accessory's counter id returned (0 when none; the second accessory is not
// looked at once the first is). The search starts at the head of the list.
u16 BattleUnit::FinishProficiency(CounterList& list)
{
    u16 finished = 0;
    bool found = false;

    for (const ObjectRecord& accessory : this->accessories)
    {
        const ObjectData& data = accessory.data;

        if (data.act == 0)
        {
            continue;
        }

        if (found)
        {
            return finished;
        }

        for (CounterNode* node = &list; node != nullptr; node = node->next.get())
        {
            if (static_cast<u16>(node->id) == data.act && node->done == 0 && node->value >= data.counterMax)
            {
                found = true;
                node->value = data.counterMax;
                node->done = 1;
                finished = data.act;
                break;
            }
        }
    }

    return finished;
}

// swd32.exe 0x00476600 (unmatched in swd3e): a refinable enemy (MissedMark) turned into one of its objects by a role of
// `level` with the
// talisman brush proficiency `proficiency`: below 50 its first object, 50..79 one of the first two, 80..100 one of
// three at random; a roll of 0..99 below 60 (90 when `level` is at least the enemy's) loads it into `object` and ends
// the mark. The object's unk48 when it is of kind bit kRefinedKind; 0 otherwise.
u16 BattleUnit::RefineTalisman(BattleServices& services, Database& database, u16 proficiency, u8 level)
{
    if ((this->marks & BattleUnit::MissedMark) == 0)
    {
        return 0;
    }

    u32 choices = 1;

    if (proficiency >= kTwoChoices && proficiency <= kTwoChoicesEnd)
    {
        choices = 2;
    }

    if (proficiency >= kThreeChoices && proficiency <= kThreeChoicesEnd)
    {
        choices = kRefineObjects;
    }

    const auto choice = static_cast<u16>(services.random.Below(choices) + 1) - 1;
    const ObjectData& data = this->data->data;
    const u16 item = choice == 0 ? data.itemA : choice == 1 ? data.itemB : choice == 2 ? data.itemC : u16{0};

    if (item == 0)
    {
        return 0;
    }

    const u16 chance = level >= data.level ? kSureRefine : kRefineChance;

    if (static_cast<u16>(services.random.Below(kRefineRoll)) >= chance)
    {
        return 0;
    }

    database.LoadObject(this->object, item);
    this->marks = static_cast<u16>(this->marks & ~BattleUnit::MissedMark);

    if ((this->object.data.kind & kRefinedKind) == 0)
    {
        return 0;
    }

    return this->object.data.unk48;
}

// swd32.exe 0x0046E090 (unmatched in swd3e): autoControl (only its address is used: battle talk command 0x3A tests it)
i32 BattleUnit::Unk2B00() const
{
    return this->autoControl;
}

// swd32.exe 0x0046E0A0 (unmatched in swd3e): guestControl (only its address is used: battle talk command 0x3A tests it)
i32 BattleUnit::Unk2B04() const
{
    return this->guestControl;
}

// swd3e.exe 0x0049B6D0: the party record's hit points as a share of `scale` (truncated)
i32 BattleUnit::HitPointShare(int scale) const
{
    const double share = static_cast<double>(this->roleRecord->hitPoints) /
                         static_cast<double>(this->roleRecord->maxHitPoints) * static_cast<double>(scale);
    return crt::Ftol(share);
}

// swd3e.exe 0x0049B700: with equipment flag Unk36 the hit number of the last target slot is taken back once: its
// amount off the hit damage, its digits cleared (unk2FD1 -2, unk306C 1). Not while panel flag 0x800 is set or once
// spent; 1 while it holds.
int BattleUnit::CancelHitNumber(BattleServices& services)
{
    if ((services.state.panelFlags & BattleState::UnkPanel800) != 0 || this->unk2FD1 < 0)
    {
        return 0;
    }

    if (this->unk306C == 1)
    {
        return 1;
    }

    if ((this->equipmentFlags & BattleUnit::Unk36) == 0)
    {
        return 0;
    }

    this->unk306C = 0;

    if (this->unk2FD1 <= 0)
    {
        this->unk2FD1 = kNumberSpent;
        return 0;
    }

    // The slot before targetCount; at 0 the original reads the four words before the digits (deviations.md).
    if (this->targetCount != 0)
    {
        auto& digits = this->damageDigits[static_cast<std::size_t>(this->targetCount - 1)];
        services.state.hitDamage -= digits[3] + kTens * (digits[2] + kTens * (digits[1] + kTens * digits[0]));
        digits = {};
    }

    this->unk2FD1 = kNumberTaken;
    this->unk306C = 1;
    return 1;
}

// swd3e.exe 0x0049B7D0
void BattleUnit::ClearUnk2FD1()
{
    this->unk2FD1 = 0;
}

// swd3e.exe 0x0049B7E0: a lasting poison held more than three turns
bool BattleUnit::PoisonLasted() const
{
    return (this->unk26B8 & kFlag26B8Poison) != 0 && this->unk2FA0 > kLastedTurns;
}

// swd3e.exe 0x0049B800
i16 BattleUnit::PoisonBase() const
{
    return this->poisonBase;
}

// swd3e.exe 0x0049B960: a turn of a lasting poison counted
void BattleUnit::CountPoisonTurn()
{
    if ((this->unk26B8 & kFlag26B8Poison) != 0)
    {
        ++this->unk2FA0;
    }
}

// swd3e.exe 0x0049B980
void BattleUnit::RaiseUnk26B8Bit28()
{
    this->unk26B8 |= kFlag26B8Bit28;
}

// swd3e.exe 0x0049B990
i32 BattleUnit::PartyMember() const
{
    return this->partyMember;
}

// swd3e.exe 0x0049B9A0
i32 BattleUnit::AutoControl() const
{
    return this->autoControl;
}

// swd3e.exe 0x0049B9B0
i32 BattleUnit::GuestControl() const
{
    return this->guestControl;
}

// swd3e.exe 0x0049B9C0
i32 BattleUnit::Ghost() const
{
    return this->ghost;
}

// swd3e.exe 0x0049B9D0: a ripple over the whole screen (a 200 x 200 grid) splashed at (100, 50) and moved to 0x50
// left of and 0x78 above the unit, act 0x17E7 in slot act 10; unk26B8 bit 20 marks it (DrawEffects runs it)
void BattleUnit::StartRipple(BattleServices& services)
{
    services.state.unk5772C8 = 1;
    services.state.unk53BFC0 = 1;
    this->unk26B8 |= kFlag26B8Ripple;
    services.state.ripple =
        std::make_unique<ScriptObject>(Display::Width, Display::Height, 0, 0, kRippleGrid, kRippleGrid);
    services.state.ripple->Splash(kRippleSplashX, kRippleSplashY, kRippleRadius, kRippleDepth, services.crtRandom);
    services.state.ripple->MoveTo(this->placement.x - kRippleLeft, this->placement.y - kRippleUp);
    this->slotActs[kRippleSlot].id = kRippleAct;
    this->rippleFrames = 0;
}

// swd3e.exe 0x0049BAB0: the enrage counters: a party role's event 2 sets enrageCounter2FD6 to 1; an enemy counts
// events 0 and 2 (or spends the counter on events 1 and 3) when its condition is the matching one (4..7)
int BattleUnit::CountEnrageEvent(int event)
{
    if (this->kind == BattleUnit::PartyRole)
    {
        if (event == kEnrageEvent2)
        {
            this->enrageCounter2FD6 = 1;
        }

        return 1;
    }

    const u16 condition = this->data->data.enrageCondition;

    switch (event)
    {
        case kEnrageEvent0:
            if (condition == kCounter2FD5Condition)
            {
                ++this->enrageCounter2FD5;
            }

            break;

        case kEnrageEvent1:
            if (condition == kSpent2FD5Condition)
            {
                this->enrageCounter2FD5 = kCounterSpent;
            }

            break;

        case kEnrageEvent2:
            if (condition == kCounter2FD6Condition)
            {
                ++this->enrageCounter2FD6;
            }

            break;

        case kEnrageEvent3:
            if (condition == kSpent2FD6Condition)
            {
                this->enrageCounter2FD6 = kCounterSpent;
            }

            break;

        default:
            break;
    }

    return event;
}

// swd3e.exe 0x0049C290
u8 BattleUnit::EnrageElement() const
{
    return this->enrageElement;
}

// swd3e.exe 0x0049C2B0
u8 BattleUnit::CommandTurns() const
{
    return this->command.turns;
}

// swd3e.exe 0x0049C310
void BattleUnit::SetUnk26B8Bit19(int on)
{
    if (on == 1)
    {
        this->unk26B8 |= kFlag26B8Bit19;
    }
    else
    {
        this->unk26B8 &= ~kFlag26B8Bit19;
    }
}

// swd3e.exe 0x0049C340: set by enrage 0x3F2
int BattleUnit::Unk26B8Bit23() const
{
    return (this->unk26B8 & kFlag26B8Bit23) != 0 ? 1 : 0;
}

// swd3e.exe 0x0049C350
i32 BattleUnit::RaiseUnk26B8(i32 bits)
{
    this->unk26B8 |= static_cast<u32>(bits);
    return bits;
}

// swd3e.exe 0x0049C370
i32 BattleUnit::ClearUnk26B8(i32 bits)
{
    this->unk26B8 &= ~static_cast<u32>(bits);
    return ~bits;
}

// swd3e.exe 0x0049C390
int BattleUnit::DamageTransferred() const
{
    return (this->status & BattleUnit::DamageTransfer) != 0 ? 1 : 0;
}

// swd3e.exe 0x0049C3A0: the command brings a rise, invisibility or a barrier, has action flag 12, or the unit's own
// object has kind bit 2
int BattleUnit::CommandSupports() const
{
    const u32 buffs = static_cast<u32>(this->command.unk08);

    if ((buffs & kSupportBuffs) != 0 || (this->command.actionFlags & UnitCommand::UnkAction1000) != 0)
    {
        return 1;
    }

    return (LowByte(this->object.data.kind) & kSupportKind) != 0 ? 1 : 0;
}

// swd3e.exe 0x0049C410: bit 15 of the command's ailment
int BattleUnit::CommandRefreshOnly() const
{
    return (this->command.ailment & SkillScript::RefreshOnly) != 0 ? 1 : 0;
}

// swd3e.exe 0x0049C420
bool BattleUnit::HasStatus(u32 bits) const
{
    return (bits & this->status) != 0;
}

}  // namespace swd3e
