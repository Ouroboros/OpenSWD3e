#include <algorithm>
#include <array>
#include <cstring>
#include <string>

#include "audio/audio_manager.h"
#include "battle/battle_enemy.h"
#include "battle/battle_message.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/battle_unit.h"
#include "battle/battle_unit_constants.h"
#include "battle/smooth/smooth_draw.h"
#include "config/config.h"
#include "core/format.h"
#include "core/words.h"
#include "crt/msvc.h"
#include "party/party.h"
#include "random/random.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e
{

namespace
{

// The messages of the effects: at (0x118, 0xA) for 0x32 frames with sound 0x117.
constexpr int kEffectMessageX = 0x118;
constexpr int kEffectMessageY = 0xA;
constexpr i16 kEffectMessageFrames = 0x32;
constexpr int kEffectSound = 0x117;
constexpr int kEndFlags = BattleMessage::BlackBar | BattleMessage::Stacked | BattleMessage::Centered;
constexpr int kFramedEndFlags = BattleMessage::Frame | BattleMessage::Stacked | BattleMessage::Centered;
constexpr int kStartFlags = BattleMessage::BlackBar | BattleMessage::Exclusive | BattleMessage::Centered;
constexpr int kPlainFlags = BattleMessage::BlackBar | BattleMessage::Centered;

// The texts of the effects (swd3e; "$1".."$3" pick the colours of BattleMessages::colors).
constexpr std::u8string_view kResistAllStart = u8"$1%s$0抵抗所有攻擊啟動";  // 0x004C0194

struct ElementTexts
{
    std::u8string_view party;  // the whole party
    std::u8string_view enemies;
    std::u8string_view one;
};

// AddBuff: +50 to the resistance of element 1..5.
constexpr std::array<ElementTexts, 5> kResistUp = {{
    {u8"$1我方所有角色$0增加50%%金象抗力", u8"$1敵方所有角色$0增加50%%金象抗力", u8"$1%s$0增加50%%金象抗力"},
    {u8"$1我方所有角色$0增加50%%木象抗力", u8"$1敵方所有角色$0增加50%%木象抗力", u8"$1%s$0增加50%%木象抗力"},
    {u8"$1我方所有角色$0增加50%%水象抗力", u8"$1敵方所有角色$0增加50%%水象抗力", u8"$1%s$0增加50%%水象抗力"},
    {u8"$1我方所有角色$0增加50%%火象抗力", u8"$1敵方所有角色$0增加50%%火象抗力", u8"$1%s$0增加50%%火象抗力"},
    {u8"$1我方所有角色$0增加50%%土象抗力", u8"$1敵方所有角色$0增加50%%土象抗力", u8"$1%s$0增加50%%土象抗力"},
}};  // 0x004C0170..0x004BFFB4

constexpr ElementTexts kDispelled = {
    u8"$1我方角色$0所有的輔助效果被消除了!!",  // 0x004BFF8C
    u8"$1敵方角色$0所有的輔助效果被消除了!!",  // 0x004BFF64
    u8"$1%s$0所有的輔助效果被消除了!!",        // 0x004BFF44
};

constexpr ElementTexts kDefenseDownStart = {
    u8"$1我方所有角色$0防禦力$3下降",  // 0x004BFEE4
    u8"$1敵方所有角色$0防禦力$3下降",  // 0x004BFEC4
    u8"$1%s$0防禦力$3下降",            // 0x004BFEB0
};

constexpr ElementTexts kSpeedDownStart = {
    u8"$1我方所有角色$0速度$3下降",  // 0x004BFE64
    u8"$1敵方所有角色$0速度$3下降",  // 0x004BFE48
    u8"$1%s$0速度$3下降",            // 0x004BFE34
};

constexpr ElementTexts kAttackDownStart = {
    u8"$1我方所有角色$0攻擊力$3下降",  // 0x004BFDFC
    u8"$1敵方所有角色$0攻擊力$3下降",  // 0x004BFDDC
    u8"$1%s$0攻擊力$3下降",            // 0x004BFDC8
};

constexpr ElementTexts kAttackUpStart = {
    u8"$1我方所有角色$0攻擊力$2上升",  // 0x004BFD68
    u8"$1敵方所有角色$0攻擊力$2上升",  // 0x004BFD48
    u8"$1%s$0攻擊力$2上升",            // 0x004BFD34
};

constexpr ElementTexts kSpeedUpStart = {
    u8"$1我方所有角色$0速度$2上升",  // 0x004BFD18
    u8"$1敵方所有角色$0速度$2上升",  // 0x004BFCFC
    u8"$1%s$0速度$2上升",            // 0x004BFCE8
};

constexpr ElementTexts kDefenseUpStart = {
    u8"$1我方所有角色$0防禦力$2上升",  // 0x004BFCC8
    u8"$1敵方所有角色$0防禦力$2上升",  // 0x004BFCA8
    u8"$1%s$0防禦力$2上升",            // 0x004BFC94
};

constexpr std::u8string_view kEarthAttackUpStart = u8"$1%s$0土象攻擊力$2上升";  // 0x004BFF2C
constexpr std::u8string_view kDamageTransferStart = u8"$1%s$0傷害轉移啟動";     // 0x004BFF18
constexpr std::u8string_view kStrongAttackStart = u8"$1%s$0強攻擊啟動";         // 0x004BFF04
constexpr std::u8string_view kWaterAttackUpStart = u8"$1%s$0水象攻擊力$2上升";  // 0x004BFE98
constexpr std::u8string_view kFireAttackUpStart = u8"$1%s$0火象攻擊力$2上升";   // 0x004BFE80
constexpr std::u8string_view kWoodAttackUpStart = u8"$1%s$0木象攻擊力$2上升";   // 0x004BFE1C
constexpr std::u8string_view kBarrierStart = u8"$1%s$0護罩效果啟動";            // 0x004BFDB4
constexpr std::u8string_view kMetalAttackUpStart = u8"$1%s$0金象攻擊力$2上升";  // 0x004BFD9C
constexpr std::u8string_view kInvisibleStart = u8"$1%s$0隱形效果啟動";          // 0x004BFD88

// ExpireEffects: the ends of the effects.
constexpr std::array<std::u8string_view, 5> kResistUpEnd = {
    u8"$1%s$0增加金象抗力解除",  // 0x004C020C
    u8"$1%s$0增加木象抗力解除",  // 0x004C01F4
    u8"$1%s$0增加水象抗力解除",  // 0x004C01DC
    u8"$1%s$0增加火象抗力解除",  // 0x004C01C4
    u8"$1%s$0增加土象抗力解除",  // 0x004C01AC
};

constexpr std::u8string_view kSpeedUpEnd = u8"$1%s$0增加速度效果解除";            // 0x004C0224
constexpr std::u8string_view kAttackUpEnd = u8"$1%s$0增加攻擊力效果解除";         // 0x004C023C
constexpr std::u8string_view kDefenseUpEnd = u8"$1%s$0增加防禦力效果解除";        // 0x004C0258
constexpr std::u8string_view kBarrierEnd = u8"$1%s$0護罩效果解除";                // 0x004C0274
constexpr std::u8string_view kAttackDownEnd = u8"$1%s$0降低攻擊力效果解除";       // 0x004C0288
constexpr std::u8string_view kInvisibleEnd = u8"$1%s$0隱形效果解除";              // 0x004C02A4
constexpr std::u8string_view kMetalAttackUpEnd = u8"$1%s$0增加金象攻擊效果解除";  // 0x004C02B8
constexpr std::u8string_view kWaterAttackUpEnd = u8"$1%s$0增加水象攻擊效果解除";  // 0x004C02D4
constexpr std::u8string_view kWoodAttackUpEnd = u8"$1%s$0增加木象攻擊效果解除";   // 0x004C02F0
constexpr std::u8string_view kFireAttackUpEnd = u8"$1%s$0增加火象攻擊效果解除";   // 0x004C030C
constexpr std::u8string_view kStrongAttackEnd = u8"$1%s$0強攻擊效果解除";         // 0x004C0328
constexpr std::u8string_view kDefenseDownEnd = u8"$1%s$0降低防禦力效果解除";      // 0x004C0340
constexpr std::u8string_view kSpeedDownEnd = u8"$1%s$0降低速度效果解除";          // 0x004C035C
constexpr std::u8string_view kDamageTransferEnd = u8"$1%s$0傷害轉移效果解除";     // 0x004C0374
constexpr std::u8string_view kEarthAttackUpEnd = u8"$1%s$0增加土象攻擊效果解除";  // 0x004C038C

// AddAilment.
constexpr u32 kAilmentRoll = 10;      // the ailment lands on a roll of 1..10 up to the chance
constexpr int kResistanceShare = 20;  // the water and wood resistances move the roll by a twentieth
constexpr int kPercent = 100;
constexpr i32 kAilmentAct = 0x2AF8;  // the act shown over an ailing unit, by its action
constexpr i32 kHeldAction = 1;       // ailment 1
constexpr i32 kAilment80Action = 2;
constexpr i32 kAilment20Action = 4;
constexpr i32 kAilment10Action = 5;
constexpr i32 kAilment200Action = 6;
constexpr i32 kAilment8Action = 0;
constexpr i32 kAilment200SlotAct = 0x1B8B;  // the act ailment 0x200 starts in slot act 10
constexpr std::size_t kEffectSlot = 10;     // the slot act of the effects (ailment 0x200, the enrage)
constexpr u16 kStatsCut = 20;               // ailment 0x200 cuts the stats of a party role by 20 percent
constexpr u16 kSoftenedTurns = 3;           // a poison already there gains a third of the new turns
constexpr float kPoisonStart = 50.0F;       // the level a lasting poison starts at
constexpr float kAilment100Start = 20.0F;
constexpr float kAilment40Start = 5.0F;
constexpr u16 kLastingTurns = 0xFFFF;
constexpr u32 kRefreshBlockingStatus = BattleUnit::UnkStatus400 | BattleUnit::StatsScaled | BattleUnit::UnkStatus100;
constexpr u32 kRefreshBlockingStatus2 = BattleUnit::UnkStatus20 | BattleUnit::UnkStatus2 | BattleUnit::UnkStatus1;

// The icons of the effects (picture 0x23B6, frame icon - 1).
enum EffectIcon : u16
{
    PoisonIcon = 1,
    Ailment2Icon = 2,
    Ailment100Icon = 3,
    Ailment4Icon = 4,
    SlowedIcon = 5,
    Ailment20Icon = 6,
    LastingPoisonIcon = 7,
    HeldIcon = 8,
    Ailment10Icon = 9,
    StatsScaledIcon = 0xA,
    Ailment80Icon = 0xB,
    Ailment8Icon = 0xC,
    Ailment400Icon = 0xD,
    AttackUpIcon = 0xE,
    DefenseUpIcon = 0xF,
    SpeedUpIcon = 0x10,
    AttackDownIcon = 0x11,
    DefenseDownIcon = 0x12,
    SpeedDownIcon = 0x13,
};

// AddBuff.
constexpr i8 kResistStep = 50;
constexpr i8 kResistMost = 100;
constexpr float kAttackUpLevel = 200.0F;
constexpr i16 kAttackUpShift = 200;
constexpr float kDefenseUpLevel = 200.0F;
constexpr i16 kDefenseUpShift = 200;
constexpr float kSpeedUpLevel = 40.0F;
constexpr i16 kSpeedUpShift = 40;
constexpr float kSpeedDownLevel = 30.0F;
constexpr i16 kSpeedDownShift = 30;
constexpr std::array<float, 3> kAttackDownScale = {1.0F, 1.5F, 2.0F};  // by a 0..2 roll
constexpr double kAttackDownBase = 60.0;
constexpr u32 kDefenseDownRoll = 3;  // the defence falls by (2..4) * 5
constexpr i16 kDefenseDownLeast = 2;
constexpr double kDefenseDownStep = 5.0;
constexpr u32 kAnyBuff = 0xFFFF4000;  // the buff and lowering bits DispelBuffs takes off

// TickEffects.
constexpr u16 kFastTick = 0x20;  // the timer step of ailments 0x40 and 0x100
constexpr u16 kTick = 0xE;
constexpr float kPoisonRise = 7.0F;
constexpr double kSlowedTotal = 15.0;
constexpr float kSlowedMost = 20.0F;
constexpr double kAilment100Total = 20.0;
constexpr float kStatsCutMost = 80.0F;
constexpr u32 kStatsCutRoll = 10;
constexpr u16 kStatsCutLeast = 10;
constexpr double kAttackUpTotal = 200.0;
constexpr double kDefenseUpTotal = 200.0;
constexpr double kSpeedUpTotal = 40.0;
constexpr double kSpeedDownTotal = 30.0;
constexpr i16 kBarrierStamina = 10;  // a barrier takes 10 stamina a turn from a party role
constexpr u16 kBlockedDamage = 9999;

// EnrageCheck: the conditions (ObjectData::enrageCondition).
enum EnrageCondition : u16
{
    LowHitPoints = 1,  // below a third of the maximum
    Cause2 = 2,
    Counter2FD3Five = 3,
    Counter2FD5Fifteen = 4,
    Counter2FD5Spent = 5,
    Counter2FD6Five = 6,
    Counter2FD6Spent = 7,
    Cause8 = 8,
    Cause9 = 9,
    ElementMetal = 0xA,
    ElementWood = 0xB,
    ElementWater = 0xC,
    ElementFire = 0xD,
    ElementEarth = 0xE,
    Cause0F = 0xF,
    Status20 = 0x10,
    Status10 = 0x11,
    Status80 = 0x12,
    Cause13 = 0x13,
    Cause14 = 0x14,
    Cause15 = 0x15,
    Cause16 = 0x16,
    Cause17 = 0x17,
    Cause18 = 0x18,
    Cause19 = 0x19,
    Cause1A = 0x1A,
    Cause1B = 0x1B,
    Counter2FD8Two = 0x1C,
    Cause1D = 0x1D,
    Cause1E = 0x1E,
};

constexpr u8 kCounter2FD3Limit = 5;
constexpr i8 kCounter2FD5Limit = 15;
constexpr i8 kCounter2FD6Limit = 5;
constexpr i8 kCounter2FD8Limit = 2;
constexpr i8 kCounterSpent = -1;    // the byte 0xFF
constexpr i32 kLowShare = 3;        // enrage condition 1: below a third of the maximum hit points
constexpr u32 kEnraged = 0x200000;  // unk2BC0: the enrage condition was met

// Enrage: the effects (ObjectData::enrageEffect).
enum EnrageEffect : u16
{
    AttackUp70 = 0x3E9,  // the stat rises by 70 percent
    DefenseUp70 = 0x3EA,
    AgilityUp70 = 0x3EB,
    AttackDoubledDefenseHalved = 0x3EC,
    AttackDoubledAgilityHalved = 0x3ED,
    DefenseDoubledAttackHalved = 0x3EE,
    DefenseDoubledAgilityHalved = 0x3EF,
    AgilityDoubledAttackHalved = 0x3F0,
    AgilityDoubledDefenseHalved = 0x3F1,
    Unk3F2 = 0x3F2,           // unk2BC0 bit 23
    InvisibleEnrage = 0x3F3,  // the unit turns invisible once the enrage act ends (EnrageFrame)
    BurstDamage = 0x3F4,      // enrageDamage, unk2BC0 bit 16
    FirstElement = 0x3F5,     // 0x3F5..0x3FA: enrageElement 1..6
    LastElement = 0x3FA,
    FirstSkill = 0x3E9,  // below it: the skill the enemy takes up
    LastEffect = 0x1770,
};

constexpr u16 kEnrageRise = 70;
constexpr u32 kEnrage3F2 = 0x800000;  // unk2BC0
constexpr u32 kEnrage3F4 = 0x10000;   // unk2BC0
constexpr double kBurstScale = 1.4;
constexpr u16 kEnrageSkillCount = 10000;  // the uses of the skill an enrage gives
constexpr u8 kEnrageSkillChance = 8;
constexpr i16 kEnrageDark = -12;  // the field darkens to -12 in each channel
constexpr i32 kEnrageAct = 0x1C98;
constexpr u32 kEnrageDone = 0x400000;  // unk2BC0: the enrage act ended
constexpr u8 kEnrageInvisibleTurns = 0xFF;

// The resistances of the elements (RoleRecord and ObjectData, swd3e: 金 木 水 火 土).
constexpr std::size_t kMetalResistance = 0;
constexpr std::size_t kWoodResistance = 1;
constexpr std::size_t kWaterResistance = 2;
constexpr std::size_t kEarthResistance = 4;

// CanTakeEffect: the ailments and buffs each ailment keeps off.
constexpr u32 kInvisibleOrResistAll = SkillScript::Invisible | SkillScript::ResistAllBuff;
constexpr u16 kAilment2Blocked = 0x700;
constexpr u16 kAilment2Blocked2 = 0x21;
constexpr u8 kAilment8Blocked = 0x27;
constexpr u16 kStatsScaledOpen = 0xC0;  // under StatsScaled only these (or a lasting poison) take hold
constexpr u16 kStatsScaledOpen2 = 0x1500;
constexpr u16 kAilment1Blocked = 0x26;
constexpr u32 kResistAllOpen = 0xE8180000;  // under ResistAll only these buffs take hold
constexpr u16 kInvisibleBlocked = 0x60;
constexpr u16 kInvisibleBlocked2 = 0x1700;
constexpr u8 kAilment400Blocked = 6;
constexpr u16 kOverlayWiping = 0x8000;  // overlayWipe: the overlay wipes in
constexpr int kPoisonFifth = 20;        // percent of the base a poison level takes off
constexpr u8 kPoisonLevels = 4;
constexpr u16 kNamedPoisonTurns = 10;

}  // namespace

// The announcement of an effect: `format` with the unit's name into its text buffer (kept by the message), the message
// and sound 0x117.
void BattleUnit::Announce(BattleServices& services, std::u8string_view format, int flags)
{
    const std::string_view name(this->displayName.data(), this->displayName.size());
    const std::string text = WinFormat(format, {name.substr(0, name.find('\0'))});
    const std::size_t length = std::min<std::size_t>(text.size(), this->text.size() - 1);
    std::memcpy(this->text.data(), text.data(), length);
    this->text[length] = '\0';
    services.messages.Add(kEffectMessageX, kEffectMessageY, kEffectMessageFrames, this->text, flags);
    services.audio.PlaySound(kEffectSound, services.config.soundVolume);
}

// swd3e.exe 0x00493AD0: whether `skill` may take hold on the unit, by the effects it has: some of its ailments are
// cured by the skill instead (a cure here answers 0), others keep the skill off.
int BattleUnit::CanTakeEffect(BattleServices& services, const SkillScript& skill)
{
    const u32 status = this->status;
    const u16 ailment = skill.ailment;
    const u32 buffs = skill.statusBits;

    if ((status & BattleUnit::Slowed) != 0)
    {
        if ((ailment & SkillScript::UnkAilment100) != 0)
        {
            this->CureEffects(services, BattleUnit::Slowed);
            return 0;
        }

        if ((ailment & SkillScript::PoisonAilment) != 0 || (buffs & SkillScript::Invisible) != 0)
        {
            return 0;
        }
    }

    if ((status & BattleUnit::Poisoned) != 0)
    {
        if ((ailment & SkillScript::SlowedAilment) != 0)
        {
            this->CureEffects(services, BattleUnit::Poisoned);
            return 0;
        }

        if ((ailment & SkillScript::UnkAilment4) != 0 || (buffs & SkillScript::Invisible) != 0 ||
            (this->unk2BC0 & kFlag2BC0Poison) != 0)
        {
            return 0;
        }
    }

    if ((status & BattleUnit::UnkStatus2) != 0)
    {
        if ((ailment & SkillScript::UnkAilment4) != 0)
        {
            this->CureEffects(services, BattleUnit::UnkStatus2);
            return 0;
        }

        if ((ailment & kAilment2Blocked) != 0 || (ailment & kAilment2Blocked2) != 0 ||
            (buffs & kInvisibleOrResistAll) != 0)
        {
            return 0;
        }
    }

    if ((status & BattleUnit::UnkStatus100) != 0)
    {
        if ((ailment & SkillScript::UnkAilment2) != 0)
        {
            this->CureEffects(services, BattleUnit::UnkStatus100);
            return 0;
        }

        if ((ailment & SkillScript::SlowedAilment) != 0 || (buffs & SkillScript::Invisible) != 0)
        {
            return 0;
        }
    }

    if ((status & BattleUnit::UnkStatus4) != 0)
    {
        // Under ailment 4 nothing takes hold: a skill with ailment 0x1000 cures it first.
        if ((HighByte(ailment) & HighByte(SkillScript::PoisonAilment)) != 0)
        {
            this->CureEffects(services, BattleUnit::UnkStatus4);
        }

        return 0;
    }

    if ((status & BattleUnit::UnkStatus8) != 0 &&
        ((LowByte(ailment) & kAilment8Blocked) != 0 || (buffs & SkillScript::Invisible) != 0))
    {
        return 0;
    }

    if ((status & BattleUnit::StatsScaled) != 0)
    {
        if ((ailment & kStatsScaledOpen) == 0 && (ailment & kStatsScaledOpen2) == 0 &&
            (this->unk2BC0 & kFlag2BC0Poison) == 0)
        {
            return 0;
        }

        return 1;
    }

    if ((status & BattleUnit::UnkStatus1) != 0 &&
        ((ailment & kAilment1Blocked) != 0 || (ailment & SkillScript::StatsScaledAilment) != 0))
    {
        return 0;
    }

    if ((status & BattleUnit::UnkStatus20) != 0 && (buffs & kInvisibleOrResistAll) != 0)
    {
        return 0;
    }

    if ((status & BattleUnit::ResistAll) != 0)
    {
        return (buffs & kResistAllOpen) != 0 ? 1 : 0;
    }

    if ((this->marks & BattleUnit::InvisibleMark) != 0 &&
        ((ailment & kInvisibleBlocked) != 0 || (ailment & kInvisibleBlocked2) != 0))
    {
        return 0;
    }

    if ((this->unk2BC0 & kFlag2BC0Poison) != 0 && (HighByte(ailment) & HighByte(SkillScript::PoisonAilment)) != 0)
    {
        return 0;
    }

    if ((this->marks & BattleUnit::UnkMark400) != 0 &&
        ((LowByte(ailment) & kAilment400Blocked) != 0 || (buffs & kInvisibleOrResistAll) != 0))
    {
        return 0;
    }

    return 1;
}

// swd3e.exe 0x00492380: the ailment of `skill` on the unit. None on an auto-controlled unit, a missed one, without
// an ailment, with skill flag 7, under DamageTransfer or when CanTakeEffect keeps it off; then a 1..10 roll above the
// ailment chance (moved by the water and wood resistances for ailment 4) resists it. An ailment already there gets
// the skill's turns and value again (a poisoned unit a third of the turns more) unless status bits 0..1, 5, 8..10
// are set; a new one (not RefreshOnly) goes in front with its icon, act and status. Then the enrage conditions are
// checked. 1 once past the roll.
int BattleUnit::AddAilment(BattleServices& services, const SkillScript& skill)
{
    if (this->autoControl == 1 || (this->marks & BattleUnit::MissedMark) != 0 || skill.ailment == 0 ||
        (LowByte(skill.flags) & SkillScript::NoAilmentFlag) != 0 || (this->status & BattleUnit::DamageTransfer) != 0)
    {
        return 0;
    }

    if (this->CanTakeEffect(services, skill) == 0)
    {
        return 0;
    }

    int roll = static_cast<int>(services.random.Below(kAilmentRoll)) + 1;
    const u8 kind = LowByte(skill.ailment);

    // The water (third) and wood (second) resistances move the roll of ailment 4.
    auto resisted = [&roll, kind](i8 water, i8 wood, int chance)
    {
        if ((kind & SkillScript::UnkAilment4) == 0)
        {
            return roll > chance;
        }

        if (water != 0)
        {
            if (roll > 0)
            {
                roll += water / -kResistanceShare;
            }

            if (roll > chance)
            {
                return true;
            }
        }

        return wood != 0 && roll + water / kResistanceShare > chance;
    };

    if (this->kind == BattleUnit::PartyRole)
    {
        const RoleRecord& record = *this->battleRecord;

        if (resisted(
                static_cast<i8>(record.resistances[kWaterResistance]),
                static_cast<i8>(record.resistances[kWoodResistance]),
                static_cast<u16>(record.ailmentChance)
            ))
        {
            return 0;
        }
    }
    else
    {
        const ObjectData& data = this->data->data;

        if (resisted(
                static_cast<i8>(data.resistances[kWaterResistance]),
                static_cast<i8>(data.resistances[kWoodResistance]),
                data.ailmentChance
            ))
        {
            return 0;
        }
    }

    // The head node is looked at first (its bits are 0 unless the memory after the list head says otherwise).
    EffectNode* found = nullptr;

    for (EffectNode* node = &this->effects; node != nullptr; node = node->next.get())
    {
        if (node->bits == skill.ailment)
        {
            found = node;
            break;
        }
    }

    if (found != nullptr && (this->status & kRefreshBlockingStatus) == 0 &&
        (this->status & kRefreshBlockingStatus2) == 0)
    {
        if ((this->status & BattleUnit::Poisoned) != 0)
        {
            found->turns = static_cast<u16>(found->turns + skill.turns / kSoftenedTurns);
        }
        else
        {
            found->turns = skill.turns;
            found->value = HighWord(skill.unk0C);
            found->timer = 0;
        }
    }

    if ((skill.ailment & SkillScript::RefreshOnly) != 0)
    {
        return 1;
    }

    if (found == nullptr)
    {
        auto added = std::make_unique<EffectNode>();
        added->next = std::move(this->effects.next);
        EffectNode& node = *added;
        this->effects.next = std::move(added);
        node.turns = skill.turns;
        node.turnsMax = skill.turns;
        node.bits = skill.ailment;
        node.value = HighWord(skill.unk0C);
        this->StartAilment(services, node);
    }

    this->EnrageCheck(services, 0, 0);
    return 1;
}

// The ailment of a new effect node starting (part of AddAilment): its act, icon and level, the unit's status and marks.
void BattleUnit::StartAilment(BattleServices& services, EffectNode& node)
{
    auto act = [&node](i32 action)
    {
        node.act.id = kAilmentAct;
        node.act.action = action;
    };

    switch (node.bits)
    {
        case SkillScript::UnkAilment1:
            act(kHeldAction);
            node.icon = HeldIcon;
            this->marks = static_cast<u16>(this->marks | BattleUnit::Held);
            this->status |= BattleUnit::UnkStatus1;
            break;

        case SkillScript::UnkAilment2:
        {
            node.icon = Ailment2Icon;
            this->marks = static_cast<u16>(this->marks | BattleUnit::UnkMark2 | BattleUnit::Held);
            this->overlayWipe = static_cast<u16>(this->overlayWipe | kOverlayWiping);
            this->status |= BattleUnit::UnkStatus2;
            // The earth resistance shortens it, the metal one lengthens it (in percent, signed: 0x00492710, the
            // compare 0x0049272D signed too).
            const auto earth = static_cast<i8>(
                this->kind == BattleUnit::PartyRole ? this->battleRecord->resistances[kEarthResistance]
                                                    : this->data->data.resistances[kEarthResistance]
            );
            const auto metal = static_cast<i8>(
                this->kind == BattleUnit::PartyRole ? this->battleRecord->resistances[kMetalResistance]
                                                    : this->data->data.resistances[kMetalResistance]
            );

            if (earth != 0)
            {
                const int cut = node.turns * earth / kPercent;
                node.turns = node.turns >= cut ? static_cast<u16>(node.turns - cut) : u16{0};
            }

            if (metal != 0)
            {
                node.turns = static_cast<u16>(node.turns + node.turns * metal / kPercent);
            }

            break;
        }

        case SkillScript::UnkAilment4:
            this->status |= BattleUnit::UnkStatus4;
            node.icon = Ailment4Icon;
            this->marks = static_cast<u16>(this->marks | BattleUnit::Held);
            this->overlayWipe = static_cast<u16>(this->overlayWipe | kOverlayWiping);
            break;

        case SkillScript::UnkAilment8:
            act(kAilment8Action);
            node.icon = Ailment8Icon;
            this->marks = static_cast<u16>(this->marks | BattleUnit::Held);
            this->status |= BattleUnit::UnkStatus8;
            break;

        case SkillScript::UnkAilment10:
            act(kAilment10Action);
            node.icon = Ailment10Icon;
            this->marks = static_cast<u16>(this->marks | BattleUnit::UnkMark100);
            this->status |= BattleUnit::UnkStatus10;
            break;

        case SkillScript::UnkAilment20:
            act(kAilment20Action);
            node.icon = Ailment20Icon;
            this->status |= BattleUnit::UnkStatus20;
            break;

        case SkillScript::SlowedAilment:
            node.icon = SlowedIcon;
            node.level = kAilment40Start;
            this->status |= BattleUnit::Slowed;
            break;

        case SkillScript::UnkAilment80:
            act(kAilment80Action);
            node.icon = Ailment80Icon;
            this->status |= BattleUnit::UnkStatus80;
            break;

        case SkillScript::UnkAilment100:
            node.icon = Ailment100Icon;
            node.level = kAilment100Start;
            this->status |= BattleUnit::UnkStatus100;
            break;

        case SkillScript::StatsScaledAilment:
        {
            act(kAilment200Action);
            this->slotActs[kEffectSlot].id = kAilment200SlotAct;
            this->status |= BattleUnit::StatsScaled;
            node.icon = StatsScaledIcon;
            node.level = kAilment100Start;
            this->unk2BE8 = 0;
            this->unk2EB2 = 0;
            this->target = 0;
            this->ResetTurn(services);

            // An enemy has no battle record: the original reads its stats through a null pointer (deviations.md).
            if (this->battleRecord == nullptr)
            {
                break;
            }

            RoleRecord& record = *this->battleRecord;
            this->savedStats = {
                record.attackBonus,
                record.defenseBonus,
                static_cast<u16>(record.agility),
                static_cast<u16>(record.wisdom)
            };

            if (this->kind == BattleUnit::PartyRole)
            {
                record.attackBonus = static_cast<u16>(record.attackBonus - kStatsCut * record.attackBonus / kPercent);
                record.defenseBonus =
                    static_cast<u16>(record.defenseBonus - kStatsCut * record.defenseBonus / kPercent);
                record.agility =
                    static_cast<i16>(record.agility - kStatsCut * static_cast<u16>(record.agility) / kPercent);
                record.wisdom =
                    static_cast<i16>(record.wisdom - kStatsCut * static_cast<u16>(record.wisdom) / kPercent);
            }

            break;
        }

        case SkillScript::UnkAilment400:
            this->status |= BattleUnit::UnkStatus400;
            node.icon = Ailment400Icon;
            break;

        case SkillScript::PoisonAilment:
            this->poisonBase = static_cast<i16>(node.value);
            this->poisonLevel = 0;
            node.turnsMax = node.turns;
            node.level = static_cast<float>(node.turns);
            this->status |= BattleUnit::Poisoned;
            node.icon = PoisonIcon;
            break;

        case SkillScript::LastingPoisonAilment:
            this->unk2BC0 |= kFlag2BC0Poison;
            this->poisonBase = static_cast<i16>(node.value);
            this->poisonLevel = 0;
            node.bits = SkillScript::PoisonAilment;
            node.turnsMax = node.turns;
            node.level = kPoisonStart;
            node.turns = kLastingTurns;
            this->status |= BattleUnit::Poisoned;
            node.icon = LastingPoisonIcon;
            break;

        default:
            break;
    }
}

// swd3e.exe 0x0049B810: a lasting poison of `base` at `level` put in front of the effects (none while a lasting one
// holds): its damage is the base less a fifth of it per level (at most four fifths); a named enemy's wears off after
// ten turns. The tail of the effects is walked first, for nothing.
int BattleUnit::AddPoison(BattleServices& services, i16 base, u8 level)
{
    if ((this->unk2BC0 & kFlag2BC0Poison) != 0)
    {
        return 0;
    }

    this->poisonLevel = static_cast<u8>(level + 1);
    this->poisonBase = base;
    auto added = std::make_unique<EffectNode>();
    added->next = std::move(this->effects.next);
    EffectNode& node = *added;
    this->effects.next = std::move(added);
    node.turns = kLastingTurns;
    node.bits = SkillScript::PoisonAilment;
    const int fifth = kPoisonFifth * this->poisonBase / kPercent;
    node.level = kPoisonStart;
    node.value = static_cast<u16>(this->poisonBase - this->poisonLevel * fifth);
    node.timer = static_cast<u16>(services.state.gaugeFull - 1);

    if (this->poisonLevel > kPoisonLevels)
    {
        node.value = static_cast<u16>(this->poisonBase - kPoisonLevels * (kPoisonFifth * this->poisonBase / kPercent));
    }

    if (this->kind == 0 && (this->data->data.kind & ObjectData::UnkKind20) != 0)
    {
        node.turns = kNamedPoisonTurns;
    }

    this->unk2BC0 |= kFlag2BC0Poison;
    this->status |= BattleUnit::Poisoned;
    node.icon = LastingPoisonIcon;
    return 1;
}

// swd3e.exe 0x00492BC0: the buff of `skill` (its status bits, or with ElementBuff a resistance of its element): none on
// an auto-controlled unit. ResistAllBuff only sets ResistAll for the skill's turns. Past CanTakeEffect, an effect with
// the same bits (or an element equal to the skill's flags) gets the new turns; else a new one goes last (not for
// DispelBuffs). Its element raises that resistance by 50 (at most 100); then its buff starts: a rise cures the
// matching fall (and the other way), the new effect taken off again; otherwise the status, the strength and the
// message. The messages of a whole side's effect name the side.
int BattleUnit::AddBuff(BattleServices& services, const SkillScript& skill)
{
    const u32 buff = skill.statusBits;

    if (buff == 0 && (skill.flags & SkillScript::ElementBuff) == 0)
    {
        return 0;
    }

    if (this->autoControl == 1)
    {
        return 0;
    }

    if ((buff & SkillScript::ResistAllBuff) != 0)
    {
        this->status |= BattleUnit::ResistAll;
        this->barrierTurns = skill.turns;
        this->Announce(services, kResistAllStart, kStartFlags);
        return 1;
    }

    if (this->CanTakeEffect(services, skill) == 0)
    {
        return 0;
    }

    EffectNode* node = &this->effects;
    EffectNode* before = nullptr;  // the effect a new one was put after (the original leaves it unset otherwise)
    bool appended = false;

    for (;;)
    {
        if (node->bits != 0 && node->bits == buff)
        {
            node->turns = skill.turns;
            break;
        }

        if (node->element != 0 && node->element == skill.flags)
        {
            node->turns = skill.turns;
            break;
        }

        if (node->next == nullptr)
        {
            if ((buff & SkillScript::DispelBuffs) == 0)
            {
                before = node;
                node->next = std::make_unique<EffectNode>();
                node = node->next.get();
                node->turns = skill.turns;
                node->turnsMax = skill.turns;
                node->value = HighWord(skill.unk0C);

                if (buff != 0)
                {
                    node->bits = buff;
                }

                if ((skill.flags & SkillScript::ElementBuff) != 0)
                {
                    node->element = skill.element;
                }

                appended = true;
            }

            break;
        }

        node = node->next.get();
    }

    const bool party = this->kind == BattleUnit::PartyRole;
    const bool wholeSide = services.state.wholeSide == 1;

    auto announce = [&](const ElementTexts& texts)
    {
        this->Announce(services, wholeSide ? (party ? texts.party : texts.enemies) : texts.one, kStartFlags);
    };

    if (skill.element >= SkillScript::Metal && skill.element <= SkillScript::Earth)
    {
        const auto element = static_cast<std::size_t>(skill.element - SkillScript::Metal);
        u8& resistance = party ? this->roleRecord->resistances[element] : this->data->data.resistances[element];
        resistance = static_cast<u8>(resistance + kResistStep);

        if (static_cast<i8>(resistance) > kResistMost)
        {
            resistance = static_cast<u8>(kResistMost);
        }

        announce(kResistUp[element]);
    }

    // A rise taking the matching fall off (or the other way): the new effect is freed again and the fall cured. The
    // original frees an effect found already there too, leaving it in the list (deviations.md): it is taken out here.
    auto cancel = [&](u32 cured)
    {
        if (appended)
        {
            before->next.reset();
        }
        else
        {
            for (EffectNode* at = &this->effects; at->next != nullptr; at = at->next.get())
            {
                if (at->next.get() == node)
                {
                    at->next = std::move(node->next);
                    break;
                }
            }
        }

        this->CureEffects(services, cured);
        return 1;
    };

    switch (buff)
    {
        case SkillScript::DefenseUp:
            if ((this->status & BattleUnit::DefenseDown) != 0)
            {
                return cancel(SkillScript::DefenseDown);
            }

            node->act.action = 0;

            if (party)
            {
                node->icon = DefenseUpIcon;
            }

            this->status |= BattleUnit::DefenseUp;
            node->level = kDefenseUpLevel;
            this->defenseShift = kDefenseUpShift;
            announce(kDefenseUpStart);
            return 1;

        case SkillScript::SpeedUp:
            if ((this->status & BattleUnit::SpeedDown) != 0)
            {
                return cancel(SkillScript::SpeedDown);
            }

            node->act.action = 0;

            if (party)
            {
                node->icon = SpeedUpIcon;
            }

            this->status |= BattleUnit::SpeedUp;
            node->level = kSpeedUpLevel;
            this->speedShift = kSpeedUpShift;
            announce(kSpeedUpStart);
            return 1;

        case SkillScript::AttackUp:
            if ((this->status & BattleUnit::AttackDown) != 0)
            {
                return cancel(SkillScript::AttackDown);
            }

            node->act.action = 0;

            if (party)
            {
                node->icon = AttackUpIcon;
            }

            this->status |= BattleUnit::AttackUp;
            node->level = kAttackUpLevel;
            this->attackShift = kAttackUpShift;
            announce(kAttackUpStart);
            return 1;

        case SkillScript::Invisible:
            node->act.action = 0;
            this->marks = static_cast<u16>(this->marks | BattleUnit::InvisibleMark);

            if (this->kind == 0)
            {
                return 1;
            }

            this->Announce(services, kInvisibleStart, kPlainFlags);
            return 1;

        case SkillScript::MetalAttackUp:
            this->Announce(services, kMetalAttackUpStart, kPlainFlags);
            return 1;

        case SkillScript::Barrier:
            node->act.action = 0;
            this->status |= BattleUnit::Barrier;

            if (this->kind == 0)
            {
                return 1;
            }

            this->Announce(services, kBarrierStart, kPlainFlags);
            return 1;

        case SkillScript::AttackDown:
        {
            if ((this->status & BattleUnit::AttackUp) != 0)
            {
                return cancel(SkillScript::AttackUp);
            }

            node->act.action = 0;

            if (party)
            {
                node->icon = AttackDownIcon;
            }

            this->status |= BattleUnit::AttackDown;
            const u32 roll = services.random.Below(static_cast<u32>(kAttackDownScale.size()));
            this->attackShift = static_cast<i16>(roll);
            const double level = static_cast<double>(kAttackDownScale[roll]) * kAttackDownBase / skill.turns;
            node->level = static_cast<float>(level);
            this->attackShift = static_cast<i16>(crt::Ftol(level));
            announce(kAttackDownStart);
            return 1;
        }

        case SkillScript::WoodAttackUp:
            this->Announce(services, kWoodAttackUpStart, kPlainFlags);
            return 1;

        case SkillScript::SpeedDown:
            if ((this->status & BattleUnit::SpeedUp) != 0)
            {
                return cancel(SkillScript::SpeedUp);
            }

            node->act.action = 0;

            if (party)
            {
                node->icon = SpeedDownIcon;
            }

            this->status |= BattleUnit::SpeedDown;
            node->level = kSpeedDownLevel;
            this->speedShift = kSpeedDownShift;
            announce(kSpeedDownStart);
            return 1;

        case SkillScript::FireAttackUp:
            this->Announce(services, kFireAttackUpStart, kPlainFlags);
            return 1;

        case SkillScript::WaterAttackUp:
            this->Announce(services, kWaterAttackUpStart, kPlainFlags);
            return 1;

        case SkillScript::DefenseDown:
        {
            if ((this->status & BattleUnit::DefenseUp) != 0)
            {
                return cancel(SkillScript::DefenseUp);
            }

            node->act.action = 0;

            if (party)
            {
                node->icon = DefenseDownIcon;
            }

            this->status |= BattleUnit::DefenseDown;
            const double level =
                static_cast<double>(static_cast<i16>(services.random.Below(kDefenseDownRoll) + kDefenseDownLeast)) *
                kDefenseDownStep;
            node->level = static_cast<float>(level);
            this->defenseShift = static_cast<i16>(crt::Ftol(level));
            announce(kDefenseDownStart);
            return 1;
        }

        case SkillScript::StrongAttackBuff:
            node->act.action = 0;
            this->status |= BattleUnit::StrongAttack;

            if (this->kind == 0)
            {
                return 1;
            }

            this->Announce(services, kStrongAttackStart, kPlainFlags);
            return 1;

        case SkillScript::DamageTransferBuff:
            node->act.action = 0;
            this->status |= BattleUnit::DamageTransfer;

            if (this->kind == 0)
            {
                return 1;
            }

            this->Announce(services, kDamageTransferStart, kPlainFlags);
            return 1;

        case SkillScript::EarthAttackUp:
            this->Announce(services, kEarthAttackUpStart, kPlainFlags);
            return 1;

        case SkillScript::DispelBuffs:
        {
            // Each buff or lowering is cured, the walk starting over from the first effect after each.
            EffectNode* at = this->effects.next.get();

            while (at != nullptr)
            {
                if ((at->bits & kAnyBuff) != 0)
                {
                    this->CureEffects(services, at->bits);
                    at = this->effects.next.get();
                    continue;
                }

                at = at->next.get();
            }

            announce(kDispelled);
            return 1;
        }

        default:
            return 1;
    }
}

// swd3e.exe 0x00491EC0: one tick of the unit's effects (none while held). Each timer rises (faster for ailments 0x40
// and 0x100); a full one (gaugeFull) starts over and moves its effect: a poison raises its level by 7 and, with no
// damage pending, deals value * level / 100; ailments 0x40 and 0x100 deal value * level / 100, their level rising by
// 15 / turns to 20 or falling by 20 / turns to 0; ailment 0x200 cuts the battle stats by 10..19 percent while its level
// is below 80; the rises fall back by their total over the turns, the falls add their strength again. Ailment 4 clears
// the pending damage. A turn passes (not for ailment 4); a barrier costs a party role 10 stamina a turn (ending it
// below 10); with UnkStatus400 the last turn deals 9999. Then the finished effects expire.
void BattleUnit::TickEffects(BattleServices& services)
{
    if (this->effects.next == nullptr || this->held != 0)
    {
        return;
    }

    for (EffectNode* node = this->effects.next.get(); node != nullptr; node = node->next.get())
    {
        const bool fast =
            (node->bits & SkillScript::SlowedAilment) != 0 || (node->bits & SkillScript::UnkAilment100) != 0;
        node->timer = static_cast<u16>(node->timer + (fast ? kFastTick : kTick));

        if (static_cast<i32>(node->timer) < services.state.gaugeFull)
        {
            continue;
        }

        node->timer = 0;

        if ((node->bits & SkillScript::PoisonAilment) != 0 && this->pendingDamage == 0)
        {
            node->level += kPoisonRise;
            const i32 damage = crt::Ftol(static_cast<double>(node->value) * static_cast<double>(node->level));
            this->pendingDamage = static_cast<u16>(damage / kPercent);
        }

        if ((LowByte(node->bits) & SkillScript::SlowedAilment) != 0)
        {
            // The step stays on the FPU stack while the damage (the value times the level) is taken.
            const double step = kSlowedTotal / node->turnsMax;
            const i32 damage = crt::Ftol(static_cast<double>(node->value) * static_cast<double>(node->level));
            this->pendingDamage = static_cast<u16>(damage / kPercent);

            if (node->level < kSlowedMost)
            {
                node->level = static_cast<float>(step + static_cast<double>(node->level));
            }
        }

        if ((node->bits & SkillScript::UnkAilment100) != 0)
        {
            const double step = kAilment100Total / node->turnsMax;
            const i32 damage = crt::Ftol(static_cast<double>(node->value) * static_cast<double>(node->level));
            this->pendingDamage = static_cast<u16>(damage / kPercent);

            if (node->level > 0.0F)
            {
                node->level = static_cast<float>(static_cast<double>(node->level) - step);
            }
        }

        if ((node->bits & SkillScript::StatsScaledAilment) != 0 && node->level < kStatsCutMost)
        {
            const u16 cut = static_cast<u16>(services.random.Below(kStatsCutRoll) + kStatsCutLeast);

            // An enemy has no battle record: the original writes through a null pointer.
            if (this->battleRecord != nullptr)
            {
                RoleRecord& record = *this->battleRecord;
                // 0x004920E0: each product is taken in 32 bits before the division.
                record.attackBonus = static_cast<u16>(record.attackBonus - cut * record.attackBonus / kPercent);
                record.defenseBonus = static_cast<u16>(record.defenseBonus - cut * record.defenseBonus / kPercent);
                record.agility = static_cast<i16>(record.agility - cut * static_cast<u16>(record.agility) / kPercent);
                record.wisdom = static_cast<i16>(record.wisdom - cut * static_cast<u16>(record.wisdom) / kPercent);
            }

            node->level = static_cast<float>(static_cast<double>(cut) + static_cast<double>(node->level));
        }

        if ((node->bits & SkillScript::AttackUp) != 0)
        {
            node->level = static_cast<float>(static_cast<double>(node->level) - kAttackUpTotal / node->turnsMax);
            this->attackShift = static_cast<i16>(crt::Ftol(static_cast<double>(node->level)));
        }

        if ((node->bits & SkillScript::DefenseUp) != 0)
        {
            node->level = static_cast<float>(static_cast<double>(node->level) - kDefenseUpTotal / node->turnsMax);
            this->defenseShift = static_cast<i16>(crt::Ftol(static_cast<double>(node->level)));
        }

        if ((node->bits & SkillScript::SpeedUp) != 0)
        {
            node->level = static_cast<float>(static_cast<double>(node->level) - kSpeedUpTotal / node->turnsMax);
            this->speedShift = static_cast<i16>(crt::Ftol(static_cast<double>(node->level)));
        }

        if ((node->bits & SkillScript::AttackDown) != 0)
        {
            this->attackShift = static_cast<i16>(this->attackShift + crt::Ftol(static_cast<double>(node->level)));
        }

        if ((node->bits & SkillScript::DefenseDown) != 0)
        {
            this->defenseShift = static_cast<i16>(this->defenseShift + crt::Ftol(static_cast<double>(node->level)));
        }

        if ((node->bits & SkillScript::SpeedDown) != 0)
        {
            node->level = static_cast<float>(static_cast<double>(node->level) - kSpeedDownTotal / node->turnsMax);
            this->speedShift = static_cast<i16>(crt::Ftol(static_cast<double>(node->level)));
        }

        if ((this->status & BattleUnit::UnkStatus4) != 0)
        {
            this->pendingDamage = 0;
        }

        if (node->turns == 0)
        {
            continue;
        }

        if ((LowByte(node->bits) & SkillScript::UnkAilment4) == 0)
        {
            --node->turns;
        }

        if (node->bits == SkillScript::Barrier && this->partyMember == 1)
        {
            if (this->roleRecord->stamina < kBarrierStamina)
            {
                node->turns = 0;
            }
            else
            {
                this->Damage(services, 0, 0, static_cast<i16>(-kBarrierStamina));
            }
        }

        if ((this->status & BattleUnit::UnkStatus400) != 0 && node->turns == 0)
        {
            this->pendingDamage = kBlockedDamage;
        }
    }

    this->ExpireEffects(services, 0);
}

// swd3e.exe 0x00493CD0: whether an effect after the head node holds the attack buff of element `kind` (1..5: metal,
// wood, water, fire, earth), or the bits `kind` itself for other values
int BattleUnit::HasEffectKind(u16 kind) const
{
    u32 bits = kind;

    switch (kind)
    {
        case SkillScript::Metal:
            bits = SkillScript::MetalAttackUp;
            break;

        case SkillScript::Wood:
            bits = SkillScript::WoodAttackUp;
            break;

        case SkillScript::Water:
            bits = SkillScript::WaterAttackUp;
            break;

        case SkillScript::Fire:
            bits = SkillScript::FireAttackUp;
            break;

        case SkillScript::Earth:
            bits = SkillScript::EarthAttackUp;
            break;

        default:
            break;
    }

    for (const EffectNode* node = this->effects.next.get(); node != nullptr; node = node->next.get())
    {
        if (node->bits == bits)
        {
            return 1;
        }
    }

    return 0;
}

// swd3e.exe 0x00493D90: whether an effect after the head node is a buff or a lowering
int BattleUnit::HasAnyBuff() const
{
    for (const EffectNode* node = this->effects.next.get(); node != nullptr; node = node->next.get())
    {
        if ((node->bits & kAnyBuff) != 0)
        {
            return 1;
        }
    }

    return 0;
}

// swd3e.exe 0x00493DC0: the share of its turns left of the first effect after the head node holding exactly `bits`
// (0 without one)
double BattleUnit::EffectShare(u32 bits) const
{
    // The argument is a word in the original (0x00493DD4).
    const u16 wanted = static_cast<u16>(bits);

    for (const EffectNode* node = this->effects.next.get(); node != nullptr; node = node->next.get())
    {
        if (node->bits == wanted)
        {
            return static_cast<double>(node->turns) / static_cast<double>(node->turnsMax);
        }
    }

    return 0.0;
}

// swd3e.exe 0x00494770: ReviveBit revives a defeated party role (its record no longer marked, its turn reset, the
// enemies checking enrage cause 0x18); otherwise every effect sharing a bit with `bits` ends at once, its bits taken
// off `bits`, the effects expired and the search starting again from the first effect until no bit is left. 0 with
// no effects.
int BattleUnit::CureEffects(BattleServices& services, u32 bits)
{
    if (bits == BattleUnit::ReviveBit && this->kind == BattleUnit::PartyRole && this->defeated == 1)
    {
        this->roleRecord->condition = static_cast<u16>(this->roleRecord->condition & ~RoleRecord::Defeated);
        this->ResetTurn(services);
        this->defeated = 0;
        --services.state.defeatedRoles;
        EnrageEnemies(services, Cause18);
        return 1;
    }

    EffectNode* node = this->effects.next.get();

    if (node == nullptr)
    {
        return 0;
    }

    while (node != nullptr)
    {
        if ((bits & node->bits) != 0)
        {
            node->turns = 0;
            node->timer = 0;
            bits &= ~node->bits;
            this->ExpireEffects(services, 0);

            if (bits == 0)
            {
                break;
            }

            node = &this->effects;
        }

        node = node->next.get();
    }

    return 1;
}

// swd3e.exe 0x00494840: with `clearAll` 0, every effect whose turns ran out ends; with 1, every effect looked at ends.
// An ending effect loses its status (an ailment its held mark when no other holds; ailments 1 and 8 check the enrage
// causes 0x15 and 0x14; ailment 0x200 restores the saved stats of a party role), is announced (not while guardPose is
// set) and, for a resistance buff, takes the 50 off again; then it is freed. The walk starts at the head node itself,
// and the effect after a freed one is not looked at in the same pass.
void BattleUnit::ExpireEffects(BattleServices& services, int clearAll)
{
    if (this->effects.next == nullptr)
    {
        return;
    }

    const bool quiet = this->guardPose != 0;

    auto end = [&](std::u8string_view format, int flags)
    {
        if (!quiet)
        {
            this->Announce(services, format, flags);
        }
    };

    auto releaseHeld = [this](u32 others)
    {
        if ((this->status & others) == 0)
        {
            this->marks = static_cast<u16>(this->marks & ~BattleUnit::Held);
        }
    };

    std::unique_ptr<EffectNode>* link = &this->effects.next;

    for (;;)
    {
        EffectNode& node = **link;

        if (node.turns == 0 || clearAll == 1)
        {
            switch (node.bits)
            {
                case SkillScript::UnkAilment1:
                    this->status &= ~BattleUnit::UnkStatus1;
                    releaseHeld(BattleUnit::UnkStatus2 | BattleUnit::UnkStatus4 | BattleUnit::UnkStatus8);
                    this->EnrageCheck(services, Cause15, 0);
                    this->held = 0;
                    break;

                case SkillScript::UnkAilment2:
                    this->marks = static_cast<u16>(this->marks & ~BattleUnit::UnkMark2);
                    this->status &= ~BattleUnit::UnkStatus2;
                    releaseHeld(BattleUnit::UnkStatus1 | BattleUnit::UnkStatus4 | BattleUnit::UnkStatus8);
                    break;

                case SkillScript::UnkAilment4:
                    this->status &= ~BattleUnit::UnkStatus4;
                    releaseHeld(BattleUnit::UnkStatus1 | BattleUnit::UnkStatus2 | BattleUnit::UnkStatus8);
                    this->held = 0;
                    break;

                case SkillScript::UnkAilment8:
                    this->status &= ~BattleUnit::UnkStatus8;
                    releaseHeld(BattleUnit::UnkStatus1 | BattleUnit::UnkStatus2 | BattleUnit::UnkStatus4);
                    this->held = 0;
                    this->EnrageCheck(services, Cause14, 0);
                    break;

                case SkillScript::UnkAilment10:
                    this->marks = static_cast<u16>(this->marks & ~BattleUnit::UnkMark100);
                    this->status &= ~BattleUnit::UnkStatus10;
                    break;

                case SkillScript::UnkAilment20:
                    this->status &= ~BattleUnit::UnkStatus20;
                    break;

                case SkillScript::SlowedAilment:
                    this->status &= ~BattleUnit::Slowed;
                    break;

                case SkillScript::UnkAilment80:
                    this->status &= ~BattleUnit::UnkStatus80;
                    break;

                case SkillScript::UnkAilment100:
                    this->status &= ~BattleUnit::UnkStatus100;
                    break;

                case SkillScript::StatsScaledAilment:
                    this->marks = static_cast<u16>(this->marks & ~BattleUnit::UnkMark10);
                    this->status &= ~BattleUnit::StatsScaled;
                    this->unk3070 = 0;

                    if (this->kind == BattleUnit::PartyRole)
                    {
                        RoleRecord& record = *this->battleRecord;
                        record.attackBonus = this->savedStats[0];
                        record.defenseBonus = this->savedStats[1];
                        record.agility = static_cast<i16>(this->savedStats[2]);
                        record.wisdom = static_cast<i16>(this->savedStats[3]);
                        this->facing = 0;
                    }

                    break;

                case SkillScript::UnkAilment400:
                    this->status &= ~BattleUnit::UnkStatus400;
                    services.state.ailment400From.fill(0);
                    break;

                case SkillScript::PoisonAilment:
                    this->status &= ~BattleUnit::Poisoned;
                    this->unk2BC0 &= ~kFlag2BC0Poison;
                    this->unk2FA0 = 0;
                    break;

                case SkillScript::EarthAttackUp:
                    end(kEarthAttackUpEnd, kEndFlags);
                    break;

                case SkillScript::DamageTransferBuff:
                    end(kDamageTransferEnd, kEndFlags);
                    this->status &= ~BattleUnit::DamageTransfer;
                    this->defenseShift = 0;
                    break;

                case SkillScript::StrongAttackBuff:
                    end(kStrongAttackEnd, kEndFlags);
                    this->status &= ~BattleUnit::StrongAttack;
                    this->defenseShift = 0;
                    break;

                case SkillScript::DefenseDown:
                    end(kDefenseDownEnd, kEndFlags);
                    this->status &= ~BattleUnit::DefenseDown;
                    this->defenseShift = 0;
                    break;

                case SkillScript::SpeedDown:
                    end(kSpeedDownEnd, kEndFlags);
                    this->status &= ~BattleUnit::SpeedDown;
                    this->speedShift = 0;
                    break;

                case SkillScript::FireAttackUp:
                    end(kFireAttackUpEnd, kEndFlags);
                    break;

                case SkillScript::WaterAttackUp:
                    end(kWaterAttackUpEnd, kEndFlags);
                    break;

                case SkillScript::WoodAttackUp:
                    end(kWoodAttackUpEnd, kFramedEndFlags);
                    break;

                case SkillScript::MetalAttackUp:
                    end(kMetalAttackUpEnd, kFramedEndFlags);
                    break;

                case SkillScript::Barrier:
                    end(kBarrierEnd, kEndFlags);
                    this->status &= ~BattleUnit::Barrier;
                    break;

                case SkillScript::AttackDown:
                    end(kAttackDownEnd, kEndFlags);
                    this->status &= ~BattleUnit::AttackDown;
                    this->unk2BD4 &= ~kAttackDownIgnored;
                    this->attackShift = 0;
                    break;

                case SkillScript::Invisible:
                    this->marks = static_cast<u16>(this->marks | BattleUnit::UnkMark400);
                    this->status &= ~BattleUnit::Invisible;
                    this->glowLevel = 0;

                    if (this->kind != 0)
                    {
                        end(kInvisibleEnd, kEndFlags);
                    }

                    break;

                case SkillScript::DefenseUp:
                    end(kDefenseUpEnd, kEndFlags);
                    this->status &= ~BattleUnit::DefenseUp;
                    this->defenseShift = 0;
                    break;

                case SkillScript::SpeedUp:
                    end(kSpeedUpEnd, kEndFlags);
                    this->status &= ~BattleUnit::SpeedUp;
                    this->speedShift = 0;
                    break;

                case SkillScript::AttackUp:
                    end(kAttackUpEnd, kEndFlags);
                    this->status &= ~BattleUnit::AttackUp;
                    this->attackShift = 0;
                    break;

                default:
                    break;
            }

            if (node.element >= SkillScript::Metal && node.element <= SkillScript::Earth)
            {
                const auto element = static_cast<std::size_t>(node.element - SkillScript::Metal);
                end(kResistUpEnd[element], kEndFlags);

                if (this->kind == BattleUnit::PartyRole)
                {
                    this->roleRecord->resistances[element] =
                        static_cast<u8>(this->roleRecord->resistances[element] - kResistStep);
                }
                else
                {
                    this->data->data.resistances[element] =
                        static_cast<u8>(this->data->data.resistances[element] - kResistStep);
                }
            }

            std::unique_ptr<EffectNode> next = std::move(node.next);

            if (next == nullptr)
            {
                link->reset();
                return;
            }

            *link = std::move(next);
        }

        EffectNode* current = link->get();

        if (current->next == nullptr)
        {
            return;
        }

        link = &current->next;
    }
}

// swd3e.exe 0x0049BB50: whether the cause `cause` (or the element `element`, or the unit's state) meets the enrage
// condition of an enemy's object (not a party role, a held unit, a defeated one, one without a condition or one
// enraged already): unk2BC0 bit 21 marks it, and the enrage runs (it does nothing unless marked). 0 for those left out.
int BattleUnit::EnrageCheck(BattleServices& services, i16 cause, i16 element)
{
    if ((this->unk2BC0 & kEnraged) != 0 || this->kind == BattleUnit::PartyRole || (this->marks & BattleUnit::Held) != 0)
    {
        return 0;
    }

    const ObjectData& data = this->data->data;
    const u16 condition = data.enrageCondition;

    if (condition == 0 || this->dying == 1 || this->defeated == 1)
    {
        return 0;
    }

    bool met = false;

    switch (condition)
    {
        case LowHitPoints:
            met = data.hitPoints < data.maxHitPoints / kLowShare;
            break;

        case Counter2FD3Five:
            met = this->enrageCounter2FD3 >= kCounter2FD3Limit;
            break;

        case Counter2FD5Fifteen:
            met = this->enrageCounter2FD5 >= kCounter2FD5Limit;
            break;

        case Counter2FD5Spent:
            met = this->enrageCounter2FD5 == kCounterSpent;
            break;

        case Counter2FD6Five:
            met = this->enrageCounter2FD6 >= kCounter2FD6Limit;
            break;

        case Counter2FD6Spent:
            met = this->enrageCounter2FD6 == kCounterSpent;
            break;

        case ElementMetal:
        case ElementWood:
        case ElementWater:
        case ElementFire:
        case ElementEarth:
            met = element == static_cast<i16>(condition - ElementMetal + SkillScript::Metal);
            break;

        case Status20:
            met = (this->status & BattleUnit::UnkStatus20) != 0;
            break;

        case Status10:
            met = (this->status & BattleUnit::UnkStatus10) != 0;
            break;

        case Status80:
            met = (this->status & BattleUnit::UnkStatus80) != 0;
            break;

        case Counter2FD8Two:
            met = this->enrageCounter2FD8 >= kCounter2FD8Limit;
            break;

        default:
            // The other conditions are met by their cause.
            met = condition <= Cause1E && cause == static_cast<i16>(condition);
            break;
    }

    if (met)
    {
        this->unk2BC0 |= kEnraged;
    }

    this->Enrage(services);
    return 1;
}

// swd3e.exe 0x0049BE20: once marked by EnrageCheck, the enemy's object gains its enrage effect (a stat up 70 percent,
// one doubled and another halved, flags, a burst of damage, an element; or below 1001 a skill used 10000 times at
// chance 8) and the field darkens with the enrage act in slot act 10
int BattleUnit::Enrage(BattleServices& services)
{
    if ((this->unk2BC0 & kEnraged) == 0)
    {
        return 0;
    }

    ObjectData& data = this->data->data;
    const u16 effect = data.enrageEffect;

    auto raise = [](u16& stat)
    {
        stat = static_cast<u16>(stat + kEnrageRise * stat / kPercent);
    };

    auto halve = [](u16& stat)
    {
        stat = static_cast<u16>(stat - (stat >> 1));
    };

    switch (effect)
    {
        case AttackUp70:
            raise(data.attack);
            break;

        case DefenseUp70:
            raise(data.defense);
            break;

        case AgilityUp70:
            raise(data.agility);
            break;

        case AttackDoubledDefenseHalved:
            data.attack = static_cast<u16>(data.attack * 2);
            halve(data.defense);
            break;

        case AttackDoubledAgilityHalved:
            data.attack = static_cast<u16>(data.attack * 2);
            halve(data.agility);
            break;

        case DefenseDoubledAttackHalved:
            data.defense = static_cast<u16>(data.defense * 2);
            halve(data.attack);
            break;

        case DefenseDoubledAgilityHalved:
            data.defense = static_cast<u16>(data.defense * 2);
            halve(data.agility);
            break;

        case AgilityDoubledAttackHalved:
            data.agility = static_cast<u16>(data.agility * 2);
            halve(data.attack);
            break;

        case AgilityDoubledDefenseHalved:
            data.agility = static_cast<u16>(data.agility * 2);
            halve(data.defense);
            break;

        case Unk3F2:
            this->unk2BC0 |= kEnrage3F2;
            break;

        case BurstDamage:
            services.state.enrageDamage = crt::Ftol(static_cast<double>(data.maxHitPoints) * kBurstScale);
            this->unk2BC0 |= kEnrage3F4;
            break;

        default:
            if (effect >= FirstElement && effect <= LastElement)
            {
                this->enrageElement = static_cast<u8>(effect - FirstElement + 1);
            }

            break;
    }

    // A signed compare (0x0049C00F): an effect of 0x8000 or more is taken up as a skill too.
    if (static_cast<i16>(data.enrageEffect) < FirstSkill)
    {
        data.skillA = data.enrageEffect;
        data.skillACount = kEnrageSkillCount;
        data.skillChance = kEnrageSkillChance;
    }

    BattleState& battle = services.state;
    battle.darkRed = kEnrageDark;
    battle.darkGreen = kEnrageDark;
    battle.darkBlue = kEnrageDark;
    battle.darkenByTint = 1;
    battle.undarkening = 0;
    battle.unk577290 = 1;
    battle.unk577270 = 1;
    battle.unk5772BC = 1;
    battle.unk5772C8 = 1;
    battle.enraging = 1;
    this->slotActs[kEffectSlot].id = kEnrageAct;
    return 0;
}

// swd3e.exe 0x0049C0D0: one frame of an enraged enemy's enrage act (slot act 10, its sound panned left) at the unit;
// once it waits, an enrage 0x3F3 makes the unit invisible for 255 turns, the field brightens again and the enrage is
// done (unk2BC0 bit 22). A defeated unit drops the enrage instead. 0 while not enraged (or casting, unk2FEC) or with
// no act; 1 otherwise.
int BattleUnit::EnrageFrame(BattleServices& services)
{
    if ((this->unk2BC0 & kEnraged) == 0 || this->unk2FEC == 1)
    {
        return 0;
    }

    if ((this->unk2BC0 & kEnrageDone) != 0)
    {
        return 1;
    }

    ActState& act = this->slotActs[kEffectSlot];

    if (act.id == 0)
    {
        return 0;
    }

    BattleState& battle = services.state;

    auto release = [&battle]
    {
        battle.unk5772BC = 0;
        battle.unk5772C8 = 0;
        battle.unk577270 = 0;
        battle.undarkening = 1;
        battle.enraging = 0;
    };

    if (this->defeated == 1)
    {
        release();
        this->unk2BC0 &= ~kEnraged;
        act = ActState{};
        return 1;
    }

    if (services.acts.Run(act) == 0)
    {
        return 0;
    }

    services.audio.PlaySound(act.view, services.config.soundVolume);
    services.audio.SetSoundPan(act.view, -kFadePan);
    act.view = 0;
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
        return 1;
    }

    if (this->data->data.enrageEffect == InvisibleEnrage)
    {
        SkillScript invisible;
        invisible.statusBits = SkillScript::Invisible;
        invisible.turns = kEnrageInvisibleTurns;
        this->AddBuff(services, invisible);
    }

    release();
    this->unk2BC0 |= kEnrageDone;
    act = ActState{};
    return 1;
}

// swd3e.exe 0x00469FB0: every enemy not defeated checks enrage cause `cause`
void EnrageEnemies(BattleServices& services, i16 cause)
{
    for (u32 enemy = 0; enemy < services.state.enemyCount; ++enemy)
    {
        BattleUnit& unit = services.enemies[enemy];

        if (!unit.IsDefeated())
        {
            unit.EnrageCheck(services, cause, 0);
        }
    }
}

}  // namespace swd3e
