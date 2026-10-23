#pragma once

#include <cstddef>

#include "core/types.h"

// The constants of the battle units that battle_unit.cpp and smooth/smooth_unit.cpp share.
namespace swd3e
{

constexpr u16 kShadowAction = 0x24;
constexpr u16 kDefeatedShadowAction = 0x27;
constexpr u32 kFlag26B8 = 0x80000000u;
constexpr u32 kFlag26B8Damage = 0x2000;      // set with kFlag26B8 when damage is pending (swd3e)
constexpr u32 kFlag26B8Blocked = 0x4000000;  // the unit does not stagger (swd3e)
constexpr u32 kFlag26B8Spell = 0x8000000;    // a hit under DamageTransfer outside a cast spell (swd3e)
constexpr u32 kFlag26B8Poison = 0x20000000;  // a lasting poison holds (swd3e)
constexpr u32 kAttackDownIgnored = 0x100;    // unk26CC bit: AttackDown does not cut the attack
constexpr u32 kFlag26B8Bit19 = 0x80000;      // swd3e (0x0049C310)
constexpr u32 kFlag26B8Ripple = 0x100000;    // swd3e: the unit's ripple runs (StartRipple)
constexpr u32 kFlag26B8Bit23 = 0x800000;     // swd3e: set by enrage 0x3F2
constexpr u32 kFlag26B8Bit28 = 0x10000000;   // swd3e (0x0049B980)
constexpr u16 kStaggerBlockingEffect = 2;    // an effect with this bit keeps the unit from staggering (swd3e)
constexpr int kFadePan = 0x30;               // swd3e: every battle sound pans by 0x30 (swd32 0x10)
constexpr int kPanCentre = 0x140;            // the sound pans right from the middle of the 640-pixel screen
constexpr int kShadowShare = 3;              // the shadow is a third of the picture height high
constexpr std::size_t kShadowShearShift = 2;
constexpr int kShadowTint = -6;
constexpr int kInvisibleShadowTint = -1;
constexpr u32 kWalksHome = 4;  // unk26CC bit 2: a party role walks home after its attack
constexpr i32 kEnemyHomeAction = 0x24;
constexpr u16 kSecondHomePose = 2;
constexpr int kHurtFlash = 0x10;  // the red flash over the picture
constexpr u16 kRising = 2;        // lifts to the top
constexpr u16 kDropping = 3;      // drops back to its saved place
constexpr u16 kReadyAct = 0x1C;   // an act whose ready pose is readyAction
constexpr i16 kNumberFadeEnd = 0x30;
constexpr int kSpeeding = -1;
constexpr i32 kFlightDoubling = 0x20;
constexpr i32 kFlightGrowth = 0x30;
constexpr int kArrived = -1;

constexpr u8 kBlueFillPose = 0xA;
constexpr u8 kBurstPose = 2;
constexpr u16 kDefeatSound = 0x31;
constexpr i16 kFadedOut = -0x20;
constexpr i16 kInvisibleFadeFrames = 0x18;
constexpr i32 kRunEnd = 6;  // fadeLevel at which the run is over
constexpr i32 kRunSoundAt = 0xF;
constexpr int kRunStep = 0x19;
constexpr int kZoomUnit = 0x400;
constexpr i32 kDrainTint = 5;
constexpr i16 kParticlesDone = 100;
constexpr u16 kWobbleSound = 0xEB;
constexpr u16 kStripFlags = ParticleEffect::WithinRange | ParticleEffect::Mixed;
}  // namespace swd3e
