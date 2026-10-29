#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "org/object_record.h"
#include "pic/projector.h"
#include "res/act_state.h"

namespace swd3e
{

class BattleState;
struct BattleServices;
struct RoleRecord;
struct TalismanGhost;
struct Picture;
struct SkillScript;

// A status effect of a unit (a 0xBC-byte list node in swd3e). The unit holds the head as a node of its own (+0x2A7C):
// some walks start there.
struct EffectNode
{
    std::unique_ptr<EffectNode> next;  // +0x00
    ActState act;                      // +0x04 the act shown over the unit (an ailment's, act 0x2AF8)
    float level = 0.0F;                // +0xA8 the strength a buff or ailment builds up or loses a tick (TickEffects)
    u32 bits = 0;                      // +0xAC the status bits it holds (SkillScript::Ailment or Buff)
    u16 timer = 0;                     // +0xB0 counts to the full gauge, then a turn passes
    u16 turnsMax = 0;                  // +0xB2 the turns it started with
    u16 turns = 0;                     // +0xB4 the turns left (0xFFFF: lasting)
    u16 element = 0;                   // +0xB6 the element 1..5 of a resistance buff
    u16 icon = 0;                      // +0xB8 picture 0x23B6, frame icon - 1 (0: none)
    u16 value = 0;                     // +0xBA the value of the skill (a poison's damage)
};

// swd3e: a list of afterimages (0xD0 bytes; the unit's at +0x1244): snapshots of pictures that fade out, each drawn
// translucent at its level and tinted under it while the level lasts.
struct ImageList
{
    struct Image
    {
        static constexpr u8 Once = 1;  // `flags`: not added again for the same picture

        std::unique_ptr<Image> next;  // +0x00 (+0x04..+0xA7 unused)
        i32 x = 0;                    // +0xA8
        i32 y = 0;                    // +0xAC
        u32 mode = 0;                 // +0xB0
        u16 frame = 0;                // +0xB4
        u16 picture = 0;              // +0xB6
        u16 unkB8 = 0;                // +0xB8
        u8 flags = 0;                 // +0xBA
        i16 tintRed = 0;              // +0xBC
        i16 tintGreen = 0;            // +0xBE
        i16 tintBlue = 0;             // +0xC0
        i16 level = 0;                // +0xC2 the translucency, falling by `step` a frame
        i16 step = 0;                 // +0xC4
    };

    void Add(const Image& image);
    int Draw(BattleServices& services);
    bool Empty() const;

    std::unique_ptr<Image> first;  // +0x00
    u16 count = 0;                 // +0xC8
    u16 lastFrame = 0;             // +0xCA the last image added
    u16 lastPicture = 0;           // +0xCC
    u16 lastB8 = 0;                // +0xCE
};

// The afterimages of a moving act (0x98 bytes; in a queued action at +0x134 and eight in a unit at +0xFCC). DrawTrail
// pushes the current picture in front every third frame and draws the images fading by fadeStep.
struct AfterimageTrail
{
    static constexpr std::size_t Images = 8;
    static constexpr u16 NoPicture = 0xFFFF;
    static constexpr u16 Left = 1;     // direction: the images trail to the left
    static constexpr u16 InPlace = 2;  // direction: the images stay on the picture

    struct Image
    {
        u32 mode = 0;     // +0x0 the draw mode
        u16 picture = 0;  // +0x4 NoPicture: none
        i16 level = 0;    // +0x6 the blend level, falling by fadeStep
        i16 x = 0;        // +0x8
        i16 y = 0;        // +0xA
        i32 unk0C = 0;
    };

    std::array<Image, Images> images = {};  // +0x0 the newest first
    i32 tintRed = 0;                        // +0x80 the tint of the images under the translucent ones
    i32 tintGreen = 0;
    i32 tintBlue = 0;
    i32 unk8C = 0;
    u16 direction = 0;  // +0x90 0: the images trail to the right, Left, InPlace
    u16 started = 0;    // +0x92
    i8 count = 0;       // +0x94 the images drawn
    i8 fadeStep = 0;    // +0x95
    u8 delay = 0;       // +0x96 frames until the next image is pushed
};

// An act flying along a line, queued on a unit (0x1DC bytes; AddQueuedAction appends one, RunQueuedActions runs them).
struct QueuedAction
{
    // QueuedAction::flags bits (swd3e).
    enum Flag : u8
    {
        UnkFlag2 = 2,  // set by RunQueuedActions (0x00495FF0)
        UnkFlag4 = 4,  // given by BoomerangFrame (0x00486110), read by RunQueuedActions
    };

    std::unique_ptr<QueuedAction> next;  // swd32 +0x0
    ActState act;                        // swd32 +0x4 the flying act
    ActState impact;                     // swd32 +0x9C the act at the end (id 0: none; a negative id lands no hit)
    AfterimageTrail trail;               // swd32 +0x134 drawn while `trailed`
    i16 toX = 0;                         // +0x1E4
    i16 toY = 0;                         // +0x1E6 0: the line stays level
    i16 fromX = 0;                       // +0x1E8
    i16 fromY = 0;                       // +0x1EA 0: the line stays level
    u16 shakeFrames = 0;                 // +0x1EC frames the act shakes before it flies
    i16 steps = 0;                       // +0x1EE line steps per frame flown (0 or less: a speeding flight)
    i16 flown = 0;                       // +0x1F2 the frames flown
    u8 done = 0;                         // +0x1F4 removed at the end of RunQueuedActions
    u8 trailed = 0;                      // +0x1F5
    u8 flags = 0;                        // +0x1F6 (swd3e) QueuedAction::Flag bits
};

// The placement of a unit (0x20 bytes, copied whole into the saved placement).
struct UnitPlacement
{
    std::array<u8, 0x14> unk00 = {};
    u16 actId = 0;  // +0x14 the act of the unit
    i16 x = 0;      // +0x16
    i16 y = 0;      // +0x18
    u16 unk1A = 0;
    i32 kind = 0;  // +0x1C 1: a party role
};

static_assert(sizeof(UnitPlacement) == 0x20);
static_assert(offsetof(UnitPlacement, actId) == 0x14);
static_assert(offsetof(UnitPlacement, kind) == 0x1C);

// The command block of a unit (0x28 bytes, cleared whole).
// The rectangle GetShadowRect fills (four dwords in the original).
struct UnitRect
{
    i32 x = 0;
    i32 y = 0;
    i32 width = 0;
    i32 height = 0;
};

// The command of a unit (+0x1004, 0x2C bytes in swd3e): its skill script (SkillScript) under the names the battle
// gives the fields.
struct UnitCommand
{
    // The bits of `actionFlags` (flag n is bit n).
    enum ActionFlag : u32
    {
        UnkAction1 = 1,
        UnkAction2 = 2,      // the swarm acts aim at 0, 0 (BattleRole::SwarmSpellFrame)
        DelaysFadeIn = 4,    // the unit fades in later (BattleUnit::DelayedFadeIn)
        StaysAcross = 0x10,  // the move starts at an edge across
        StaysDown = 0x20,    // the move starts at the top
        Restores = 0x80,     // the hit restores (HitDamage): it cures and heals the target
        DrainsLife = 0x100,
        DrainsSpirit = 0x200,
        DrainsStamina = 0x400,
        UnkAction800 = 0x800,
        UnkAction1000 = 0x1000,
        UnkAction2000 = 0x2000,
        UnkAction4000 = 0x4000,
        UnkAction8000 = 0x8000,     // swd3e (ActFlag15)
        UnkAction20000 = 0x20000,   // swd3e (ActFlag17)
        ShrinksInFlight = 0x10000,  // swd3e: FlyAct draws the act shrinking
        FixedOrigin = 0x40000,      // GetOrigin places slot act 8 from (0xF0, 0x12C) (0x00491CE0)
    };

    // The bits of `flags`.
    enum Flag : u16
    {
        TargetsUnit = 1,  // a target command (ClearMoveAxes)
        UnkFlag2 = 2,
        UnkFlag4 = 4,      // swd3e: under StatusBit5 with unk2BC0 bit 28 the role stands aside (PrepareCommand)
        UnkFlag8 = 8,      // with UnkFlag20: a party role walks home after it acts (ReturnHome)
        UnkFlag10 = 0x10,  // an enemy walks home after it acts
        UnkFlag20 = 0x20,  // the walks wait for the act to follow up, then cover 1/16 (there) or 1/12 (home) a frame
        AtTarget = 0x38,   // any of bits 3..5: DrawAction places the act on the target (inferred from DrawAction)
        FadesIn = 0x18,    // bits 3 and 4: a unit with autoControl fades in before it attacks (AttackFrame)
    };

    i32 unk00 = 0;        // +0x00 (unit +0x1004)
    u32 actionFlags = 0;  // +0x04
    i32 unk08 = 0;        // +0x08
    u16 flags = 0;        // +0x0C
    i16 value = 0;        // +0x0E the value of the skill (SkillValue)
    u16 resistance = 0;   // +0x10 the element 1..9 whose resistance scales the damage
    u16 followAct = 0;    // +0x12 the follow-up action's act
    u16 castAct = 0;      // +0x14
    u16 unk16 = 0;        // +0x16
    u16 unk18 = 0;        // +0x18 (swd3e) raised by the wisdom over 50 (SelectSkill)
    u16 ailment = 0;      // +0x1A
    u16 element = 0;      // +0x1C
    u16 pose = 0;         // +0x1E the pose of a defeat (its low byte)
    i16 unk20 = 0;        // +0x20
    u16 unk22 = 0;        // +0x22
    u16 unk24 = 0;        // +0x24 (swd3e)
    u16 target = 0;       // +0x26 the first value of CommandTarget
    u16 unk28 = 0;        // +0x28 (swd3e) the second value of CommandTarget
    u8 turns = 0;         // +0x2A
    u8 unk2B = 0;         // +0x2B
};

static_assert(sizeof(UnitCommand) == 0x2C);

class Database;
class Random;
class TswArchive;

namespace smooth
{
class Scene;
class ScenePainter;
class UnitFrame;
}  // namespace smooth

class ErrorReporter;
struct CounterNode;
using CounterList = CounterNode;

class BattleUnit
{
    friend class smooth::UnitFrame;  // the rebuild's smooth battle: the frame functions split (smooth/smooth_unit.h)

public:
    // BattleUnit::marks bits.
    enum Mark : u16
    {
        UnkMark1 = 1,  // cleared when the guard text has risen
        UnkMark2 = 2,
        UnkMark4 = 4,       // the unit is drawn over a grey copy (IdleFrame)
        UnkMark8 = 8,       // cleared by ResetTurn
        UnkMark10 = 0x10,   // swd3e: no critical attack while set (MarkBit4)
        MissedMark = 0x20,  // the ailment missed (SkillScript::MissedAilment)
        Held = 0x40,
        InvisibleMark = 0x80,
        UnkMark100 = 0x100,
        UnkMark200 = 0x200,  // set by ResetTurn
        UnkMark400 = 0x400,  // set when invisibility ends
        UnkMark800 = 0x800,
        GuardingMark = 0x1000,
        DefeatedMark = 0x2000,
        Unk4000 = 0x4000,      // the gauge stops (set elsewhere in the battle loop)
        UnkMark8000 = 0x8000,  // the damage of the unit is not raised
    };

    // BattleUnit::unk2BDA bits (swd3e): the weak stand pose and the state of the talisman ghost's orders
    // (battle_ghost.cpp).
    enum GhostFlag : u16
    {
        WeakStand = 1,         // the stand pose is the weak one (IdleFrame)
        GhostGuards = 2,       // the ghost keeps by its master (order 4)
        GhostSkillUsed = 4,    // a fed skill of kind 0x1000 was chosen (TakeSkillUsed)
        GhostAttackUsed = 8,   // one of kind 8 or 0x10 (TakeAttackUsed)
        GhostHealUsed = 0x10,  // one of kind 4 (TakeHealUsed)
        GhostOrder7 = 0x20,    // order 7 was given (TakeOrder7)
        GhostSpent = 0x40,     // the ghost spent its spirit (SpendGhostSpirit)
        GhostRests = 0x80,     // order 2: the ghost rests and gains spirit
    };

    // BattleUnit::equipmentFlags set by the accessories (BattleRole::ApplyAccessory).
    enum EquipmentFlag : u32
    {
        Unk32 = 1,
        BladeReturn = 4,
        Unk36 = 8,
        SpiritDrain = 0x10,
        LifeDrain = 0x20,
        SpiritFan = 0x40,
        SpecialActEquipment = 0x80,  // swd3e: accessory kind 0x3A (BattleRole::StartSpecialAct)
        UnkEquipment100 = 0x100,     // swd3e: accessory kind 0x28 has no value (AccessoryValue)
    };

    // A unit in battle (0x2B28 bytes in the original; the base of BattleRole and BattleEnemy). Offsets of the original
    // are given for reference; fields not yet named keep their offset. BattleUnit::status bits, named by the texts the
    // buffs announce (AddBuff 0x0047BC50) and their effects.
    enum Status : u32
    {
        UnkStatus1 = 1,
        UnkStatus2 = 2,
        UnkStatus4 = 4,
        UnkStatus8 = 8,
        UnkStatus10 = 0x10,  // cleared with mark UnkMark100
        UnkStatus20 = 0x20,
        Slowed = 0x40,       // ailment 0x40: the gauge fills at half speed
        UnkStatus80 = 0x80,  // a struck unit evades 8 times in 10 (Strike)
        UnkStatus100 = 0x100,
        StatsScaled = 0x200,  // a party role's attributes are scaled (undone when it ends)
        UnkStatus400 = 0x400,
        Poisoned = 0x1000,         // swd3e: a poison (EffectNode::bits 0x1000) eats hit points a tick
        DamageTransfer = 0x20000,  // swd3e: 傷害轉移 (no ailment lands; RaiseFlag2BC0 outside a cast spell)
        StrongAttack = 0x40000,    // swd3e: 強攻擊 (a role's attacks are always critical)
        DefenseDown = 0x100000,
        AttackDown = 0x1000000,
        ResistAll = 0x2000000,  // swd3e: 抵抗所有攻擊 (no stagger)
        Barrier = 0x4000000,
        SpeedDown = 0x8000000,
        AttackUp = 0x10000000,
        SpeedUp = 0x20000000,
        DefenseUp = 0x40000000,
        Invisible = 0x80000000,
    };

    static constexpr int SlotActs = 11;
    static constexpr i32 PartyRole = 1;

    // BattleUnit::commandA / commandB values (meanings not yet known).
    enum Command : u16
    {
        UnkCommand1 = 1,  // fades in with action 0x28
        UnkCommand2 = 2,
        UnkCommand3 = 3,
        UnkCommand4 = 4,  // swd3e: an enemy takes the anchor of its stand act (GetOffset)
        UnkCommand6 = 6,
        UnkCommandD = 0xD,
        UnkCommandE = 0xE,
        UnkCommandF = 0xF,
        UnkCommand29 = 0x29,  // swd3e: goes to commandB (SetCommand)
        UnkCommand16 = 0x16,
        UnkCommand18 = 0x18,
        UnkCommand1A = 0x1A,
        UnkCommand1B = 0x1B,
        UnkCommand1C = 0x1C,
        UnkCommand50 = 0x50,  // swd3e: drawn with the spark act like UnkCommand6 (IdleFrame)
        UnkCommand1F = 0x1F,
        UnkCommand22 = 0x22,
        UnkCommand23 = 0x23,
        UnkCommand24 = 0x24,
        UnkCommand28 = 0x28,  // swd3e: the hit is the meter's sum (HitDamage)
    };

    // BattleUnit::stage bits: the stages of an attack (AttackFrame).
    enum Stage : u32
    {
        Entered = 1,              // swd3e: the whole dword 1: the zoomed entrance is over (ZoomEntranceFrame)
        ChargeFollowing = 0x800,  // the follow-up of a charge runs (ChargeAttackFrame)
        Recoiling = 0x4000,       // the recoil act runs (DrawRecoil)
        FollowingUp = 0x8000,     // the follow-up action runs in its slot act
    };

    // BattleUnit::commandFlags bits.
    enum CommandFlag : u16
    {
        UnkCommandFlag200 = 0x200,  // GetOffset uses slot act UnkSlot8
        UnkCommandFlag400 = 0x400,  // set by the accessory commands that take command B
        UnkCommandFlag800 = 0x800,  // set by command 6
        UnkCommandFlag1000 = 0x1000,
        UnkCommandFlag2000 = 0x2000,
        UnkCommandFlag4000 = 0x4000,  // set by commands 0xE and 0xF
        UnkCommandFlag8000 = 0x8000,  // set when the skill action is loaded
    };

    // BattleUnit::DrainKind values.
    enum Drain : u16
    {
        DrainedHitPoints = 1,
        DrainedSpirit = 2,
        DrainedStamina = 4,
        DrainedLife = 8,
    };

    // The pictures a hit pushes on its unit (PushTarget): the damage number, healing, a spirit or stamina gain.
    static constexpr u16 DamageTarget = 0x2389;
    static constexpr u16 HealTarget = 0x237D;
    static constexpr u16 SpiritGainTarget = 0x237E;
    static constexpr u16 StaminaGainTarget = 0x237F;
    static constexpr u16 ReviveBit = 0x8000;  // CureEffects: revives a defeated party role

    static constexpr std::size_t Trails = 8;
    static constexpr std::size_t Drains = 4;
    static constexpr std::array<i32, 10> DigitHops = {0, 12, 18, 12, 8, 4, 0, 2, 0, 0};  // 0x004BFB40 by hop step
    // HurtFrame: the offsets of the stagger by its step. The four tables lie one after another up to 0x004BFC2C (eight
    // words no code of swd3e reads) and are read past their ends by the later steps: each runs on through the data
    // after it, up to 0x004BFC3C.
    // 0x004BFBA4 (12 entries): the red flash of a unit of unk3070 shakes this far each way
    static constexpr std::array<i32, 38> StaggerFlash = {
        14,      14,      -24,       10,        10,        -8,        -8,       6,        6,        -4,
        -4,      0,       757736212, 825307183, 842150450, 842150450, 2621460,  5242940,  7864420,  9175180,
        9175180, 9175180, 10485840,  17039580,  19005720,  20971830,  20971840, 20316470, 18350370, 6553820,
        -655310, -655350, -655350,   -655350,   -131073,   -327683,   -131078,  65533,
    };

    // 0x004BFBD4 (16 entries): the shake back from the hit
    static constexpr std::array<i8, 104> StaggerShake = {
        20,  35, 42,  45,  47, 48,  49, 49,   50, 50,   50, 50,   50, 50,   50, 50,   20, 0,    40, 0,   60,
        0,   80, 0,   100, 0,  120, 0,  -116, 0,  -116, 0,  -116, 0,  -116, 0,  -116, 0,  -116, 0,  80,  0,
        -96, 0,  -36, 0,   4,  1,   24, 1,    34, 1,    54, 1,    64, 1,    64, 1,    64, 1,    54, 1,   54,
        1,   34, 1,   24,  1,  -36, 0,  100,  0,  50,   0,  -10,  -1, 10,   0,  -10,  -1, 10,   0,  -10, -1,
        10,  0,  -10, -1,  -1, -1,  -3, -1,   -3, -1,   -6, -1,   -6, -1,   -3, -1,   -3, -1,   0,  0,
    };

    // 0x004BFBE4 (12 entries): the slide back of a nudged unit
    static constexpr std::array<u16, 44> StaggerSlide = {
        20,  40,    60,  80,    100, 120,   140,   140,   140,   140,   140,   140,   80,    160, 220,
        260, 280,   290, 310,   320, 320,   320,   310,   310,   290,   280,   220,   100,   50,  65526,
        10,  65526, 10,  65526, 10,  65526, 65535, 65533, 65533, 65530, 65530, 65533, 65533, 0,
    };

    // 0x004BFBFC (24 entries): the lift of a unit nudged in place (SetNudge 1)
    static constexpr std::array<i16, 32> StaggerLift = {
        80, 160, 220, 260, 280, 290, 310, 320, 320, 320, 310, 310, 290, 280, 220, 100,
        50, -10, 10,  -10, 10,  -10, 10,  -10, -1,  -3,  -3,  -6,  -6,  -3,  -3,  0,
    };

    // DefeatHop: the heights of the hop by frame (0x004BFB68).
    static constexpr std::array<i32, 15> HopHeights = {0, 4, 12, 24, 32, 24, 16, 8, 4, 0, -4, -12, -16, -20, -32};
    static constexpr int UnkSlot8 = 8;
    static constexpr int UnkSlot9 = 9;
    static constexpr u32 NoTarget = 0xFFFFFFFF;
    static constexpr u16 NoUnk2EAE = 0xFFFF;
    static constexpr u16 MaxTargets = 0x23;      // pushedTargets entries
    static constexpr u16 Healing = 0x8000;       // a damage amount that heals
    static constexpr i32 Opaque = 0xF;           // fadeLevel of a fully visible unit; the fade out counts down from it
    static constexpr i32 FadedIn = 0x10;         // brightLevel when the act ends the fade in early
    static constexpr i32 DelayedFadeIn = -0x20;  // brightLevel of a unit that fades in 0x20 frames later
    static constexpr u16 FlashPeak = 0x18;       // the tint pulse rises from FlashStart to FlashPeak and falls back
    static constexpr u16 FlashStart = 2;
    static constexpr std::size_t DamageDigits = 4;  // the decimal digits of a damage number
    static constexpr std::size_t GuardLetters = 4;  // the letters of the guard text
    static constexpr u16 DefaultFadeStep = 4;       // fadeStep: the alpha step of a fade
    static constexpr u32 DefaultColorPair = 0x062B062B;
    static constexpr u32 OutlineColorPair = 0x07E007E0;  // the outline of DrawAction

    BattleUnit();
    ~BattleUnit();
    BattleUnit(const BattleUnit&) = delete;
    BattleUnit& operator=(const BattleUnit&) = delete;

    int SetTarget(int value);
    int GaugeLength(const BattleServices& services) const;
    i16 ResetGauge(const BattleServices& services);
    int RandomizeGauge(BattleServices& services, int full);
    void GetDrawPosition(u16& x, u16& y) const;
    void GetOffset(u16& x, u16& y) const;
    void GetOrigin(u16& x, u16& y) const;
    int HasFixedOrigin() const;
    void GetShadowRect(BattleServices& services, UnitRect& rect);
    i16 Move(i16 dx, i16 dy);
    i16 SetPosition(i16 x, i16 y);
    i16 GetPosition(u16& x, u16& y) const;
    const Picture* StepIdleAction(BattleServices& services);
    const void* SelectEntry(int second) const;
    int GetValue2FEC() const;
    int GetValue2FF4() const;
    u16 GetCommandA() const;
    u16 GetCommandB() const;
    u16 GetWord2F90() const;
    u16 GetWord2EAE() const;
    int ClearIdleAction();
    int SetCommand(int command);
    void ClearFlag2BC0();
    void RaiseFlag2BC0(const BattleState& battle);
    int Flag2BC0() const;
    int PushTarget(u16 target);
    int StartDefeat(int kind);
    int ResetTurn(BattleServices& services);
    int Retarget(i16 target);
    u16 FollowAct() const;
    int ActFlag15() const;
    int ActFlag17() const;
    bool HasActionFlags(u32 flags) const;
    int SetUnk3038(int value);
    void RaiseFlag2BC0Bit17();
    int Flag2BC0Bit17() const;
    bool HitPointsAtMost(int share) const;
    i16 RoleAgility() const;
    u16 RoleAttackBonus() const;
    void FillSecondGauge();
    int SecondGaugeLength() const;
    bool CanHeal() const;
    void SlideHook(int value);
    u16 TreasureLevel(const CounterList& list) const;
    int AddTreasureExperience(CounterList& list, u16 kind, u16 amount);
    bool IsWeakEnemy() const;
    int DrainSecondGauge();
    u16 Counter2F92() const;
    void IncreaseHits();
    void ClearWord2EAE();
    void RaiseUnk2FE8();
    int Unk2FE8() const;
    int SetOutline(int value);
    bool IsAE4Clear() const;
    u16 CommandUnk16() const;
    void RaiseUnk3008();
    int DrawFadeOut(BattleServices& services);
    int DrawFadeIn(BattleServices& services, const BattleUnit& anchor);
    int SetHeld(int value);
    int SetUnk3014(int value);
    int RollEvade(BattleServices& services, u16 chance);
    int Mirrored() const;
    bool IsDefeated() const;
    int StatusBit13() const;
    bool IsDefeatedOrDying() const;
    i32 Unk2B00() const;
    i32 Unk2B04() const;
    int StepTargetCount(BattleServices& services, int increase);
    int PushTargetAlt(u16 value);
    int ResetAfterGuard();
    int GetGuard() const;
    int SetStatus(int bits);
    int ClearStatus(int bits);
    i16 SetWord2A0A(i16 value);
    int SetGuarding(int value);
    int ActFlag1() const;
    int ActFlag11() const;
    i16 ActSpeed() const;
    int ActFlag0() const;
    int ActFlag13() const;
    int ActFlag14() const;
    int SetAction(i16 value);
    bool IsGuarding() const;
    void SetVitals(i16 hitPoints, i16 spirit, i16 stamina);
    i16 SetAttributes(i16 attack, i16 defense, i16 agility);
    void SetLevel(i16 level);
    int SetB00(int value);
    int SetB04(int value);
    void Reset(BattleServices& services);
    void DrawGuardText(BattleServices& services);
    int AddAilment(BattleServices& services, const SkillScript& skill);
    int AddBuff(BattleServices& services, const SkillScript& skill);
    int HasEffectElement(i16 element) const;
    int HasEffectKind(u16 kind) const;
    int CanTakeEffect(BattleServices& services, const SkillScript& skill);
    int HasAnyBuff() const;
    double EffectShare(u32 bits) const;
    int AddPoison(BattleServices& services, i16 base, u8 level);
    int EnrageCheck(BattleServices& services, i16 cause, i16 element);
    int Enrage(BattleServices& services);
    int EnrageFrame(BattleServices& services);
    int KnockBackFrame(BattleServices& services);
    int ZoomEntranceFrame(BattleServices& services, BattleUnit& anchor, u16 act);
    int CastPoseFrame(BattleServices& services, i32 slotAct);
    i32 HitPointShare(int scale) const;
    int CancelHitNumber(BattleServices& services);
    void ClearUnk2FD1();
    bool PoisonLasted() const;
    i16 PoisonBase() const;
    void CountPoisonTurn();
    void RaiseUnk2BC0Bit28();
    i32 PartyMember() const;
    i32 AutoControl() const;
    i32 GuestControl() const;
    i32 Ghost() const;
    void StartRipple(BattleServices& services);
    int CountEnrageEvent(int event);
    u8 EnrageElement() const;
    u8 CommandTurns() const;
    void SetUnk2BC0Bit19(int on);
    int Unk2BC0Bit23() const;
    i32 RaiseUnk2BC0(i32 bits);
    i32 ClearUnk2BC0(i32 bits);
    int DamageTransferred() const;
    int CommandSupports() const;
    int CommandRefreshOnly() const;
    bool HasStatus(u32 bits) const;
    i32 HitPointsAndAttack() const;
    int OutOfChoices() const;
    u8 ObjectUnkA2() const;
    int FirstResistance() const;
    int Named() const;
    void ClearTurnCommand();
    int HasEffectBits(u32 bits) const;
    bool ApplyPendingDamage(BattleServices& services);
    void DrawEffectIcons(BattleServices& services, int x, int y);
    void ExpireEffects(BattleServices& services, int clearAll);
    SkillScript CommandSkill() const;
    i16 HitDamage(BattleServices& services, BattleUnit& target, i16& spiritDamage, i16& staminaDamage);
    int DrawAction(BattleServices& services, BattleUnit& target, ActState& act);
    void AimAction(BattleServices& services, BattleUnit& target, ActState& act, i16 shiftX, i16 shiftY);
    int DrawAimed(BattleServices& services, BattleUnit& target, ActState& act, i16 shiftX, i16 shiftY);
    int RunQueuedActions(BattleServices& services, BattleUnit& target, i32 action);
    void AddQueuedAction(
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
    );
    void TrailLastQueuedAction(i32 tintRed, i32 tintGreen, i32 tintBlue);
    void DrawActionAt(BattleServices& services, i16 x, i16 y, ActState& act);
    int ReturnHome(BattleServices& services, const BattleUnit& target, i32 id, i32 action);
    int LoadEnemyCommand(Database& database, int choice, u32& act);
    int FadeInvisible(BattleServices& services, int mode);
    int StepAccessoryCounters(CounterList& list, u16 kind, u16 amount);
    int StepGauge(BattleServices& services, int boost);
    int FadeMainOut(BattleServices& services);
    int FadeMainIn(BattleServices& services);
    void DrawMainFaded(BattleServices& services, int tint);
    int Defense() const;
    int DefeatFrame(BattleServices& services);
    int EndDefeat(BattleServices& services);
    int EndParticleDefeat(BattleServices& services);
    void StartDefeatDraw(BattleServices& services, const Picture& picture);
    void DrawDefeatPiece(
        BattleServices& services,
        const Picture& picture,
        int left,
        int top,
        int right,
        int bottom,
        int x,
        int y,
        int height
    );
    int DefeatRising(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatBurst(BattleServices& services, const Picture& picture, int x, int y);
    int RunDefeatParticles(BattleServices& services);
    void DecodeForParticles(BattleServices& services, const Picture& picture, u32 line);
    int DefeatSplitRows(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatSplitColumns(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatFill(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatFadeOut(BattleServices& services, const Picture& picture);
    int DefeatQuarters(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatWave(BattleServices& services, const Picture& picture);
    int DefeatSink(BattleServices& services, const Picture& picture);
    int DefeatZoom(BattleServices& services, const Picture& picture);
    int DefeatStrips(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatHalves(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatSlide(BattleServices& services, const Picture& picture);
    int DefeatHop(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatProjected(BattleServices& services, const Picture& picture);
    int DefeatParticles(BattleServices& services, const Picture& picture, int x, int y);
    int DefeatFlight(BattleServices& services, const Picture& picture);
    int DefeatBoss(BattleServices& services, const Picture& picture);
    void DrawWobble(BattleServices& services, const Picture& picture, smooth::Scene& scene);
    int WalkToTarget(BattleServices& services, const BattleUnit& target);
    void TickEffects(BattleServices& services);
    int CanUseAccessory(u16 kind);
    void StartDrainEffect(BattleServices& services, int slot, BattleUnit& source);
    int DrainEffectFrame(BattleServices& services, int slot);
    int CatchRoll(BattleServices& services, u8 level);
    int HurtFrame(BattleServices& services);
    int HurtStep(BattleServices& services);
    void IdleFrame(BattleServices& services, int paused);
    void IdleFrameBody(BattleServices& services, smooth::Scene& scene);
    void DrawEffects(BattleServices& services, int x, int y, smooth::Scene& scene);
    void DrawFlash(smooth::ScenePainter& painter);
    int DrawPulse(const BattleServices& services, smooth::ScenePainter& painter);
    void DrawTint(smooth::ScenePainter& painter);
    void DrawShadowTinted(smooth::ScenePainter& painter);
    int ChooseCommand(BattleServices& services, Database& database, u32& act);
    int ChooseAttack(Database& database);
    void MarkDelayedAttack();
    i16 EnemyHitDamage(BattleServices& services, BattleUnit& target);
    bool FanGuards(BattleServices& services, u16 ailment);
    int RunAway(BattleServices& services);
    int StepOrbCounters(CounterList& list, const UnitCommand& command);
    int DrawDamageNumbers(BattleServices& services);
    int ItemName(Database& database, int choice, std::string& name, u32& id);
    int SkillName(Database& database, int choice, std::string& name);
    u16 RollDrop(BattleServices& services);
    u16 FinishProficiency(CounterList& list);
    u16 RefineTalisman(BattleServices& services, Database& database, u16 proficiency, u8 level);
    void DrawTrail(BattleServices& services, AfterimageTrail& trail, u16 frame, u16 picture, u32 mode, i16 x, i16 y);
    int DrawRecoil(BattleServices& services, ActState& act, i16 y);
    int FlyAct(
        BattleServices& services,
        const BattleUnit& target,
        ActState& act,
        int slot,
        i32 id,
        int fromX,
        int fromY,
        int steps,
        i16 delay,
        int backwards
    );
    int CureEffects(BattleServices& services, u32 bits);
    int Damage(BattleServices& services, int hitPoints, i16 spirit, i16 stamina);
    int EffectFlag2() const;
    int EffectFlag3() const;
    void SetDefeatPose(u8 pose);
    void SetDefeatPoseFixed(u8 pose);
    void SetDamageDigits(BattleServices& services, int amount);
    int AdvanceGauge(const BattleServices& services, int boost);
    ObjectRecord* Data() const;
    bool MarkBit11Clear() const;
    int IsHeld() const;
    u16 ActId() const;
    u16 DrainKind() const;
    int StatusBit5() const;
    int MarkBit4() const;
    i32 Unk2EB2Shown() const;
    bool Unk2EB2BelowProficiency(CounterList& proficiency);
    int SetValueAAC(int value);
    int SetFacingIfAble(int value);
    void SetNudge(u16 nudge, u16 saved);
    bool CommandTarget(u16& target, u16& second) const;
    int MarkBit1() const;
    bool MarkBit8Clear() const;
    bool HasBlock30() const;
    int Word2FB0Bit13() const;
    int Word2FB0Bit12() const;
    bool RestartMainAct(i16 value, int force);
    int CommandStatusBit() const;
    void CommandValues(u16& value, u16& turns) const;
    bool IsSmallMachine() const;
    void Unk66And90(u16& first, u16& second) const;
    u16 AccessoryValue(u16 kind) const;
    i16 AccessoryProficiency(u16 kind, CounterList& proficiency);
    void SetEnemySkills(i16 first, i16 second, i16 third, i16 fourth);
    void SetEnemyItems(i16 a, i16 b, i16 c, i16 d, i16 e, i16 f);
    void SetResistances(std::span<const i16, 5> values);
    bool ShowsPicture(i16 picture) const;
    void LoadSkillAction(Database& database, u16 id, i32& act);
    u16 PrepareCommand(Database& database);
    bool ChooseHeal(BattleServices& services);
    void ClearMoveAxes(u16& x, u16& y) const;
    void LoadCommand(Database& database, u16 id);
    int SetFacing(int value);
    void SetDisplayName(std::span<const char> name);
    void GetStamina(u16& maximum, u16& value) const;
    void GetSpirit(u16& maximum, u16& value) const;
    void GetHitPoints(i32& value, i32& maximum) const;

    // swd32.exe 0x004A7D8C (unmatched in swd3e): the height of a popping letter or digit by phase.
    static constexpr std::array<i32, 10> PopOffsets = {0, 12, 18, 12, 8, 4, 0, 2, 0, 0};

private:
    void MirrorRecoil(u32 actFlags, bool forced);
    void Announce(BattleServices& services, std::u8string_view format, int flags);
    void StartAilment(BattleServices& services, EffectNode& node);

public:
    // +0x0..+0x0F: what the unit is.
    RoleRecord* battleRecord =
        nullptr;                       // +0x0 the battle copy of a party role's record (+0x26, +0x28: attack, defence)
    RoleRecord* roleRecord = nullptr;  // +0x4 the role's party record
    i32* roleBlock = nullptr;          // +0x8 the role's 0x60-byte block at 0x004C6700 ([0]: the fury gauge, inferred)
    ObjectRecord* data = nullptr;      // +0xC an enemy's mon.dat object (usually `object`)
    TalismanGhost* ghostRecord = nullptr;     // +0x10 (swd3e) the talisman ghost's record (SetupGhost)
    std::unique_ptr<ObjectRecord> ownedData;  // a party role's block at +0x0C (freed by Reset)
    ObjectRecord object;                      // +0x14 (its description at +0xB8; ResetTurn clears it)
    ObjectRecord weapon;  // +0xBC (its skillScript at +0xFA, 0x0047EC6E) a party role's weapon (equipment slot 0)
    std::array<ObjectRecord, 2> accessories;  // +0x164 equipment slots 7 and 8

    ActState mainAct;                           // +0x2B4
    ActState standAct;                          // +0x358
    ActState castAct;                           // +0x3FC
    ActState recoilAct;                         // +0x4A0
    ActState attackAct;                         // +0x544
    ActState unk598;                            // swd32 +0x598
    std::array<ActState, SlotActs> slotActs;    // +0x68C
    ActState status100Act;                      // +0xD98 (swd3e) the act over a unit of status 0x100 (DrawEffects)
    ActState slowedAct;                         // +0xE3C (swd3e) the act over a slowed unit (DrawEffects)
    ActState stepAct;                           // +0xEE0
    UnitPlacement placement;                    // +0xFC4
    UnitPlacement savedPlacement;               // +0xFE4
    UnitCommand command;                        // +0x1004
    ProjectedPicture projected;                 // swd32 +0xDB8 the picture of defeat pose 51 (DefeatProjected)
    std::vector<u16> projectedPixels;           // its raw pixels
    LineWalker flight;                          // +0x106C the line of a flying act (FlyAct)
    ParticleEffect absorb;                      // +0x108C a picture breaking up into the unit (StartAbsorb)
    std::vector<u16> absorbPixels;              // the raw pixels of absorb (decoded by 0x004019A0, freed by 0x004885A0)
    std::array<ParticleEffect, Drains> drains;  // +0x10E4 the drain effects (StartDrainEffect)
    std::array<std::vector<u16>, Drains> drainPixels;  // their raw pixels
    ImageList images;                                  // +0x1244 (swd3e) the afterimages of a knock back
    std::array<AfterimageTrail, Trails> trails;        // +0x1314 the afterimages of the unit's acts
    std::unique_ptr<QueuedAction> actions;             // +0x2794
    const Picture* shadowPicture = nullptr;            // +0x2A40
    const Picture* picture = nullptr;                  // +0x2A44 the current picture
    const Picture* unk2550 = nullptr;                  // swd32 +0x2550
    const Picture* overlayPicture = nullptr;           // +0x2A4C the overlay drawn on its picture
    const Picture* absorbSource = nullptr;             // +0x2A54 the picture StartAbsorb breaks up
    std::span<const char> displayName;      // +0x2A58 the name shown in the messages (read up to its NUL when used)
    EffectNode effects;                     // +0x2A7C (the head is a node: +0x2A80 its act, +0x2B28 its bits)
    std::array<char, 0x30> skillName = {};  // +0x2B38 the name of the skill used
    std::array<i32, 8> flightSteps =
        {};                  // +0x2B48 the steps per frame of FlyAct by its slot (0x00495E39; callers use 0)
    i32 unk2B68 = 0;         // +0x2B68 (only cleared, by 0x004917DE)
    i32 unk2B6C = 0;         // +0x2B6C
    i32 fadeLevel = Opaque;  // +0x2B70 (also the dash offset of ChargeAttackFrame)
    i32 brightLevel = 1;     // +0x2B74
    i32 homePoseX = 0;       // +0x2B78 the x offset of the stand pose while walking home (ReturnHome)
    i32 unk2B7C = 0;
    i32 unk2B80 = 0;
    u32 stage = 0;       // +0x2B84 BattleUnit::Stage bits
    i32 throwSteps = 0;  // +0x2B88 the frames of the thrown act so far (ThrowAct)
    i32 unk2B8C = 0;
    u32 glowFlags = 0;       // +0x2B90 the draw mode of the glow act
    i32 glowOffsetX = 0;     // +0x2B94
    i32 readyAction = 0;     // +0x2B98 the action of its ready pose (kReadyAct)
    u32 drawFlags = 0;       // +0x2B9C bit 0: mirrored
    i32 overlayOffsetX = 0;  // +0x2BA0 the x offset of the overlay (IdleFrame)
    u32 frameFlags = 0;      // +0x2BA8 flag 1 toggled for mirrored frames
    u32 shadowMode = 0;      // +0x2BAC the mode its shadow is drawn with
    i32 unk2BB0 = 0;
    i32 unk2BB4 = 0;
    i32 damageCarry = 0;               // +0x2BB8
    i32 hitPointsOverride = 0;         // +0x2BBC an enemy's current hit points when set
    u32 unk2BC0 = 0;                   // +0x2BC0 bit 31
    u32 colorPair = DefaultColorPair;  // +0x2BC4
    u32 status = 0;                    // +0x2BC8 status bits (2: stone, 12: frozen, ...)
    u32 equipmentFlags = 0;            // +0x2BD0 bits 0, 2..6 from the equipment
    u32 unk2BD4 = 0;
    u16 marks = 0;      // +0x2BD8 (bit 1, 0x20, 0x40: held, 0x100, bit 13)
    u16 unk2BDA = 0;    // +0x2BDA (swd3e) bit 0: the weak stand pose (IdleFrame); bit 2 (0x00486540)
    i16 guardPose = 0;  // +0x2BDC the guard pose (1, 2, 4)
    u16 unk2BDE = 0;
    u16 nudge = 0;                                  // +0x2BE0
    u16 drawAtSaved = 0;                            // +0x2BE2
    u16 proficiency = 0;                            // +0x2BE6 the proficiency of the accessory used
    std::array<u16, MaxTargets> damageDelays = {};  // +0x2BEA (PushTargetAlt 0x004910AE)
    std::array<std::array<i16, DamageDigits>, MaxTargets> damageDigits = {};  // +0x2C30 the four digits per target slot
    std::array<std::array<i16, DamageDigits>, MaxTargets> unk2D48 =
        {};                                          // +0x2D48 the hop step of each digit (DrawDamageNumbers)
    std::array<i16, GuardLetters> guardPhases = {};  // +0x2D90 the pop phases of the four letters of the guard text
    i16 slotTint = 0;    // +0x2E60 the tint of a waiting slot act, darkening to its end (AttackFrame)
    i16 recoilTint = 0;  // +0x2E62 the tint of a waiting recoil act, darkening to its end (DrawRecoil)
    i16 glowLevel = 0;   // +0x2E64 the brightness of a glow act (GlowAct)
    std::array<i16, MaxTargets> numberFades = {};  // +0x2E66 the rise and fade of each damage number
    i16 guardRise = 0;                             // +0x2EAC
    u16 unk2EAE = NoUnk2EAE;
    u16 unk2EB0 = 0;
    // +0x2EB2 (swd3e) a count kept by HitDamage (0x00497900), cleared by ClearExtras; while positive the role has no
    // critical attack; shown as "X%2d" plus one by the panel of 0x00478FA0 against a tenth of the proficiency of
    // accessory kind 0x27
    i16 unk2EB2 = 0;
    // swd3e: the strength of the attack, defence and speed effects (AddBuff sets them, TickEffects moves them).
    i16 attackShift = 0;   // +0x2ED6
    i16 defenseShift = 0;  // +0x2ED8
    i16 speedShift = 0;    // +0x2EDA
    // swd3e: a poison (AddAilment 0x2000, AddPoison): its base damage and level (1..; 5 and up count as 4).
    i16 poisonBase = 0;  // +0x2FA2
    u8 poisonLevel = 0;  // +0x2FD2
    // swd3e: bit 15: the stone or freeze overlay wipes in (DrawEffects), the low bits its height so far.
    u16 overlayWipe = 0;  // +0x2FA4
    // swd3e: the attack and defence bonuses, the agility and the wisdom of the battle record when ailment 0x200 landed
    // (restored when it ends).
    std::array<u16, 4> savedStats = {};  // +0x2FA8
    // swd3e: the counters the enrage conditions 3..7 and 0x1C test (EnrageCheck), and the element 1..6 an enrage
    // 0x3F5..0x3FA gives.
    u8 enrageCounter2FD3 = 0;  // +0x2FD3
    i8 enrageCounter2FD5 = 0;  // +0x2FD5
    i8 enrageCounter2FD6 = 0;  // +0x2FD6
    u8 unk2FD7 = 0;            // +0x2FD7 (swd3e) only cleared (Reset)
    i8 enrageCounter2FD8 = 0;  // +0x2FD8
    u8 enrageElement = 0;      // +0x2FDC
    // +0x2FDD (swd3e) an enemy with no object counts 1, with no skill counts 2 (BattleEnemy::Setup, Reload)
    u8 unk2FDD = 0;
    // swd3e: a second gauge (0x960 when full, 0x0048B170; drawn by 0x0048B190, run down by 0x004902E0), cleared by
    // ailment 0x200
    i16 unk2BE8 = 0;  // +0x2BE8
    u16 unk2FA0 = 0;  // +0x2FA0 (swd3e) cleared when a poison ends
    // +0x3070 (swd3e) 1: an enemy whose act has no action 0x26 (0x00487D60), or set by DrawEffects; cleared when
    // ailment 0x200 ends
    i32 unk3070 = 0;
    // swd3e: with equipment flag Unk36 a hit number is taken back once (CancelHitNumber): the uses left (0x0048BA60
    // counts them down; -1: none, -2: used), and 1 once it was
    i8 unk2FD1 = 0;       // +0x2FD1
    i32 unk306C = 0;      // +0x306C
    u16 flightDelay = 0;  // +0x2FBE (swd3e) the frames FlyAct waited before its steps grow
    i16 unk2EB4 = 0;      // +0x2EB4 (swd3e) a sum kept by Damage (0x00495330)
    u16 unk2EDE = 0;      // +0x2EDE (swd3e) cleared when a flight arrives (FlyAct, WalkToTarget)
    u16 unk2EDC = 0;      // +0x2EDC (swd3e) set to 6 when a walk arrives (WalkToTarget), not read
    u8 evadeCounter = 0;  // +0x2FD4 (swd3e) the evasion rolls of an enemy of skill 4 or more since its last success
    u8 rippleFrames = 0;  // +0x2FDB (swd3e) the frames of the unit's ripple (StartRipple, DrawEffects ends it past 30)
    // swd3e: 1 once an enemy chose its objects or its skills (read by OutOfChoices with the counts left)
    u8 unk2FD9 = 0;  // +0x2FD9
    u8 unk2FDA = 0;  // +0x2FDA
    // swd3e: the text of the unit's messages, kept by address by the messages (Announce).
    std::array<char, 0x40> text = {};  // +0xF84
    u16 restoreHitPoints = 0;          // +0x2EB6 the hit points a command restores
    u16 restoreSpirit = 0;             // +0x2EB8 the spirit
    u16 restoreStamina = 0;            // +0x2EBA the stamina
    u16 actAnchorX = 0;  // +0x2EBC the act's anchor x, from the picture's right edge when mirrored (DrawAction)
    u16 aimAnchorX = 0;  // +0x2EBE the same for AimAction
    u16 overlayX = 0;    // +0x2EC0 the x of the overlay picture from the act (DrawAction)
    u16 originX = 0;     // +0x2EC2
    u16 originY = 0;     // +0x2EC4 (swd3e) the act's y offset (IdleFrame)
    u16 actOffsetX = 0;  // +0x2EC6 the act's x offset, mirrored like the picture (DrawAction)
    u16 aimOffsetX = 0;  // +0x2EC8 the act's x offset of AimAction, mirrored like the picture
    u16 actX = 0;        // +0x2ECA where DrawAction draws the act
    u16 actY = 0;        // +0x2ECC
    u16 aimX = 0;        // +0x2ECE where AimAction places the act
    u16 aimY = 0;        // +0x2ED0
    std::array<u16, MaxTargets> pushedTargets = {};  // +0x2EE0
    u16 fadeStep = DefaultFadeStep;                  // +0x2F26 the alpha step of its fades
    u16 actId = 0;                                   // +0x2F28 the unit's act
    u16 action = 0;                                  // +0x2F2A
    u16 gauge = 0;                                   // +0x2F2E the action gauge
    u16 unk2A14 = 0;
    std::array<u32, 4> targets = {NoTarget, NoTarget, NoTarget, NoTarget};  // +0x2F72
    u16 glowPhase = 0;            // +0x2F82 0: brightening, 1: dimming, 2: over (GlowAct)
    u16 flashPhase = FlashStart;  // +0x2F84 the rise of its turn flash
    u16 flashFall = FlashPeak;    // +0x2F86 and its fall
    u16 commandA = 0;             // +0x2F88
    u16 unk2F8A = 0;
    u16 commandB = 0;  // +0x2F8C commands 2, 3, 6, 0xD, 0xE, 0xF, 0x29 and 0x50
    u16 unk2F8E = 0;
    u16 unk2F90 = 0;
    u16 hurtStep = 0;  // +0x2F94 the step of the stagger (HurtFrame)
    u16 unk2F92 = 0;
    u16 drainingGauge = 0;  // +0x2F96 (swd3e) 1 while the second gauge (unk2BE8) runs down (0x004902E0)
    u16 targetCount = 0;    // +0x2F98
    u16 damageSlot = 0;     // +0x2F9A
    u16 queuedActions = 0;  // +0x2F9C the acts AddQueuedAction appended
    u16 landedActions = 0;  // +0x2F9E the queued acts that reached their end
    u16 pendingDamage = 0;  // +0x2FA6 bit 15: healing
    u16 commandFlags = 0;   // +0x2FB0 CommandFlag bits (high byte at +0x2FB1)
    u16 restAction = 0;     // +0x2FB2 the rest action of the frame (IdleFrame)
    u16 magicAction = 0;    // +0x2FB4 the action of its magic act
    u16 unk2FB6 = 0;
    u16 unk2FB8 = 0;
    u16 barrierTurns = 0;  // +0x2FBA
    u8 unk2FC4 = 0;        // +0x2FC4 (cleared by ResetUnit 0x00491690)
    u16 orbitStep = 0;     // +0x2FC0 (swd3e) the talisman ghost's step along its orbit segment (IdleFrame)
    u16 orbitSegment = 0;  // +0x2FC2 (swd3e) and the segment
    u8 evadeSkill = 0;     // +0x2FC5 an evade roll of 0..kEvadeRoll below it evades
    u8 defeatPose = 0;     // +0x2FC6 the pose of the defeat (a fixed pose overrides it)
    u8 fixedPose = 0;      // +0x2FC7
    u8 idleTurns = 0;      // +0x2FC9 the turns since the enemy chose an object or a skill (ChooseCommand)
    u8 itemWait = 0;       // +0x2FCA the turns since the third object was forced
    u8 skillWait = 0;      // +0x2FCB the turns since the second skill was forced
    u8 choicePhase = 0;    // +0x2FCC 1: objects were last chosen, 2: skills
    u8 extraAttacks = 0;   // +0x2FCD the attacks the unit repeats (set by skill 0x15)
    u8 healChoice = 0;     // +0x2FCE
    u8 healPercent = 0;    // +0x2FD0 1: restoreHitPoints..restoreStamina are percentages of the target's maxima
    i32 kind = 0;          // +0x2FE0 1: a party role
    i32 unk2FE4 = 0;
    i32 unk2FE8 = 0;
    i32 unk2FEC = 0;
    i32 unk2FF0 = 0;
    i32 unk2FF4 = 0;
    i32 defeated = 0;  // +0x2FF8
    i32 dying = 0;     // +0x2FFC 1 while the defeat runs (DefeatFrame)
    i32 outline = 0;   // +0x3000
    i32 charging = 0;  // +0x3004 1: ChargeAttackFrame is in its charge (also set when a stagger ends)
    i32 unk3008 = 0;
    i32 unk300C = 0;
    i32 held = 0;  // +0x3010
    i32 unk3014 = 0;
    i32 unk3018 = 0;
    i32 unk301C = 0;
    i32 unk3020 = 0;
    i32 target = 0;       // +0x3024
    i32 extraNumber = 0;  // +0x3028
    i32 unk302C = 0;
    i32 guarding = 0;  // +0x3030
    i32 unk3034 = 0;
    i32 unk3038 = 0;    // +0x3038 1: the gauge tick (0x0047E720) and the poison spread (0x00469D80) skip the unit
    i32 absorbing = 0;  // +0x303C 1 while an absorb runs (AbsorbFrame)
    // +0x3040 (swd3e) 1: a party role set up from its record (Setup), 0: the talisman ghost (0x0047ED60); read by
    // HitDamage, 0x0048BA60, HurtFrame and 0x004808D0
    i32 partyMember = 0;
    i32 ghost = 0;         // +0x304C (swd3e) 1: the talisman ghost in battle (0x0047ED60)
    i32 autoControl = 0;   // +0x3044 1: the role acts by itself (AI)
    i32 guestControl = 0;  // +0x3048 1: a guest role, acting by itself
    i32 facing = 0;        // +0x3050
    i32 unk3054 = 0;
    i32 fadedIn = 0;  // +0x3064 a forced unit has faded in for its attack (AttackFrame)
    i32 facingTurnsBack =
        0;           // +0x3068 set with the facing (SetFacingIfAble); 1: the facing turns back when the gauge fills
    i32 nudged = 0;  // +0x3058 (swd3e) set by SetNudge for a shake, read by HurtFrame
    i32 evadeRolled = 0;  // +0x305C 1: the evade roll of the attack was made
    i32 goldGuard = 0;    // +0x3060
};

int ApplyElement(const std::array<u8, 5>& resistances, i16 element, i16& value);
bool RollChance(Random& random, int percent);
void EnrageEnemies(BattleServices& services, i16 cause);

}  // namespace swd3e
