#pragma once

#include <memory>
#include <vector>

#include "core/types.h"
#include "core/words.h"

namespace swd3e
{

// An action script: a stream of 16-bit words, two-letter opcodes stored as ASCII (low byte first), most followed by
// one operand word. The original returns a pointer into the act entry, so reads past the script continue into the
// scripts after it in the entry: the script is kept as its entry and its offset there.
struct ActScript
{
    std::shared_ptr<const std::vector<u8>> entry;
    u32 offset = 0;

    void reset()
    {
        this->entry.reset();
        this->offset = 0;
    }

    // The bytes from the script start to the end of the entry.
    std::size_t size() const
    {
        return this->entry == nullptr || this->offset >= this->entry->size() ? 0 : this->entry->size() - this->offset;
    }

    const u8* data() const
    {
        return this->entry == nullptr ? nullptr : this->entry->data() + this->offset;
    }

    bool operator==(std::nullptr_t) const
    {
        return this->entry == nullptr;
    }
};

// A two-letter opcode as the script stores it, the first letter in the low byte.
constexpr u16 ScriptTag(char first, char second)
{
    return MakeBytes(static_cast<u8>(second), static_cast<u8>(first));
}

// Opcodes of the action scripts.
enum class ActOpcode : u16
{
    EndFrame = ScriptTag('D', 'E'),  // ends the steps of this frame
    Loop = ScriptTag('V', 'O'),      // back to the start (stays while held)
    Stop = ScriptTag('2', 'O'),      // stays here; waiting once no delay is left
    View = ScriptTag('V', 'W'),
    Position = ScriptTag('Y', 'X'),  // x, y
    Size = ScriptTag('H', 'W'),      // width, height
    Counter = ScriptTag('N', 'T'),
    Wait = ScriptTag('W', 'T'),
    Invisible = ScriptTag('I', 'V'),
    Mask = ScriptTag('M', 'S'),
    Frame = ScriptTag('F', 'R'),
    Rotation = ScriptTag('O', 'R'),
    Delay = ScriptTag('D', 'S'),
    EffectQ = ScriptTag('E', 'Q'),
    OffsetX = ScriptTag('X', 'O'),
    OffsetY = ScriptTag('Y', 'O'),
    Picture = ScriptTag('A', 'P'),    // picture number, counts the rounds of the counter
    Zoom = ScriptTag('C', 'S'),       // the zoom of both axes (Painter::SetZoom when enabled)
    LoopRange = ScriptTag('P', 'L'),  // the rounds bounding a repeat and its count
    Action = ScriptTag('A', 'O'),
    Signal = ScriptTag('S', 'G'),
    DrawLevel = ScriptTag('D', 'L'),
    Visible = ScriptTag('O', 'N'),
    Lights = ScriptTag('L', 'F'),  // seven values
    Green = ScriptTag('G', 'C'),
    ClearColors = ScriptTag('L', 'C'),
    Red = ScriptTag('R', 'C'),
    Blue = ScriptTag('B', 'C'),
    EffectA = ScriptTag('E', 'A'),
    Half = ScriptTag('H', 'A'),
    MaskBlend = ScriptTag('M', 'A'),
    NoBlend = ScriptTag('N', 'A'),
    Target = ScriptTag('T', 'A'),
    AnchorX = ScriptTag('X', 'A'),
    AnchorY = ScriptTag('Y', 'A'),
};

// Playback of an action script (0xA4 bytes in the original). Field offsets of the original are given for reference.
struct ActState
{
    // `counter`: the low byte is the round limit, the high byte the round.
    static constexpr int Lights = 7;  // the values of opcode LF
    static constexpr u16 RoundsMask = 0xFF;
    static constexpr int RoundShift = 8;
    static constexpr u16 RoundStep = 0x100;  // 'AP' adds one round
    // `loopCount` ('PL'): the repeats left in the low bits, with the bits below.
    static constexpr u16 LoopActive = 0x8000;   // set by 'PL'; a 'PL' with it set keeps the count
    static constexpr u16 LoopForever = 0x4000;  // the repeat never counts down
    static constexpr u16 LoopMask = 0x7FFF;
    static constexpr i16 FixedDelay = static_cast<i16>(0x8000);  // in `speed`: the low bits are a fixed delay
    static constexpr i32 NotSaved = -1;                          // in `savedAction` and `savedDirection`
    static constexpr i32 NoLast = -1;  // in `lastAction` and `lastDirection`: the act runs from the start

    // The act directions (RolePath::Facing maps the steps onto them).
    enum Direction : u16
    {
        Up = 0,
        Down = 1,
        Left = 2,
        Right = 3,
        UpLeft = 4,
        DownRight = 5,
        DownLeft = 6,
        UpRight = 7,
    };

    // Bits of ActState::target read by BattleUnit::AimAction and BattleRole::AttackFrame.
    enum TargetBit : u16
    {
        Strikes = 0x1,       // the act strikes the target (BattleRole::Strike)
        Recoils = 0x2,       // the unit's recoil act plays the act's effectA (action effectQ)
        UnkTarget4 = 0x4,    // raises flag 26B8 of the target, then the bits are cleared
        UnkTarget10 = 0x10,  // with Strikes: an enemy strikes without raising the flag (Game::EnemyAttackFrame)
        FollowsUp = 0x8,     // the follow-up action starts
        Dashes = 0x100,      // with FollowsUp: the role dashes (BattleRole::ChargeAttackFrame)
        Holds = 0x200,       // the act stays on its step while the unit recoils
        FadesField = 0x400,  // with FollowsUp: the field fades by the act's lights
        AddsPicture = 0x20,  // swd3e: the act's effect is added as a second-tier spell picture (AddHitPicture)
        SlidesBack = 0x40,   // swd3e: the units slide back (BattleState::StartSlideBack)
        SlidesTo = 0x800,    // swd3e: the units slide to the unit (BattleState::StartSlideTo)
        Quakes = 0x8000,     // starts the screen quake (0x0045E7D0 shakes while BattleState::quaking)
    };

    // Flags of ActState::flags.
    enum Flag : u32
    {
        Invisible = 0x1,  // 'IV' sets, 'ON' clears
        KeptOnRestart = 0x80000003u,
        KeptOnTurn = 0x80000000u,
        MaskBlend = 0x4,   // 'MA'
        Half = 0x8,        // 'HA'
        DrawLevel = 0x10,  // 'DL'
        Signal = 0x14,     // 'SG'
        NoBlend = 0x2C,    // 'NA'
    };

    i32 id = 0;               // +0x00 act id (id / 3000: archive, id % 3000: entry)
    i32 lastId = 0;           // +0x04
    i32 action = 0;           // +0x08 first sub entry of the action
    i32 lastAction = 0;       // +0x0C
    i32 x = 0;                // +0x10 'YX'
    i32 y = 0;                // +0x14
    u32 flags = 0;            // +0x18 ActState::Flag
    i32 unknown1C = 0;        // +0x1C
    i32 savedAction = 0;      // +0x20 the action before the role turned to face someone
    i32 effectA = 0;          // +0x24 'EA'
    i32 effectQ = 0;          // +0x28 'EQ'
    i32 width = 0;            // +0x2C 'HW'
    i32 height = 0;           // +0x30
    i32 direction = 0;        // +0x34 added to the action for the sub entry
    i32 lastDirection = 0;    // +0x38
    i32 savedDirection = 0;   // +0x3C the direction before it turned
    u16 counter = 0;          // +0x40 'NT': low byte the round limit, high byte the round
    u16 position = 0;         // +0x42 script position in words
    u16 delay = 0;            // +0x44 frames left on the current frame
    u16 frameDelay = 0;       // +0x46 'DS'
    i16 speed = 0;            // +0x48 FixedDelay: a fixed delay in the low bits
    u16 frame = 0;            // +0x4A 'FR'
    u16 picture = 0;          // +0x4C 'AP'
    u16 rotation = 0;         // +0x4E 'OR'
    u16 actionCode = 0;       // +0x50 'AO', cleared before every step
    u16 unknown52 = 0;        // +0x52
    ActScript script;         // +0x54
    u16 view = 0;             // +0x58 'VW'
    u16 target = 0;           // +0x5A 'TA' ActState::TargetBit
    u16 unknown5C = 0;        // +0x5C
    u16 zoomX = 0;            // +0x5E 'CS': the zoom of Painter::SetZoom (0x00423630)
    u16 zoomY = 0;            // +0x60 'CS': the same value
    u16 offsetX = 0;          // +0x62 'XO'
    u16 offsetY = 0;          // +0x64 'YO'
    u16 drawLevel = 0;        // +0x66 'DL'
    u16 red = 0;              // +0x68 'RC'
    u16 green = 0;            // +0x6A 'GC'
    u16 blue = 0;             // +0x6C 'BC'
    u16 unknown6E[3] = {};    // +0x6E
    u16 red2 = 0;             // +0x74 'RC' second value, cleared by 'LC'
    u16 green2 = 0;           // +0x76
    u16 blue2 = 0;            // +0x78
    u16 anchorX = 0;          // +0x7A 'XA'
    u16 anchorY = 0;          // +0x7C 'YA'
    u16 lights[Lights] = {};  // +0x7E 'LF'
    u16 loopCount = 0;        // +0x8C 'PL' ActState::LoopActive etc.
    u8 loopStart = 0;         // +0x8E 'PL' the round the repeat waits at
    u8 loopEnd = 0;           // +0x8F 'PL' the round that restarts the script
    u8 wait = 0;              // +0x90 'WT'
    u8 wavePhase = 0;         // +0x91 the phase its wave was drawn at (Painter::WavePhase)
    u8 signal = 0;            // +0x92 'SG'
    u8 unknown93 = 0;         // +0x93
    u8 unknown94 = 0;         // +0x94 1: 'CS' sets the zoom for this act (writers not identified)
    i32 waiting = 0;          // +0x98 '2O' reached with no delay
    i32 hold = 0;             // +0x9C 1: stay on the current step
    i32 mask = 0;             // +0xA0 'MS'
};

// Back to the start of the script: the step position, delays, speed and hold cleared.
void ResetPlayback(ActState& state);
void FillUnused(ActState& state);

}  // namespace swd3e
