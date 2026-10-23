#pragma once

#include "core/types.h"

// The constants of the battle's HUD that battle_hud.cpp and smooth/smooth_hud.cpp share.
namespace swd3e
{

constexpr int kCoinStep = 0x32;
constexpr u16 kNotLoaded = 0xFFFF;
constexpr i16 kEmptyMeter = -1;  // AddToMeter: the amount that empties the meter
constexpr u32 kMeterPicture = 0x2397;
constexpr int kMeterTint = 0xA;  // the fill is tinted while its level moves
constexpr i16 kMeterNumbers = 0x2377;
}  // namespace swd3e
