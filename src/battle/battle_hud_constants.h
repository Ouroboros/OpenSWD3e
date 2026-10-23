#pragma once

#include <string_view>

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
constexpr int kGrowUnit = 0x400;  // DrawGrowing: the zoom of the picture's own size
constexpr i32 kGrowWidthStep = 0x3C;
constexpr i32 kGrowHeightStep = 0x50;
constexpr i32 kGrowEnd = 0x4B0;
constexpr u32 kRoulettePicture = 0x23B0;  // DrawRoulette
constexpr int kRouletteFrameLeft = 0x34;
constexpr int kRouletteFrameDown = 2;
constexpr int kRouletteDigitWidth = 0xE;
constexpr u16 kRouletteSound = 0x125;
constexpr int kRouletteMessageX = 0x118;  // RouletteBonus
constexpr int kRouletteMessageY = 0xA;
constexpr i16 kRouletteMessageFrames = 0x3C;
constexpr int kRouletteStep = 10;
constexpr i8 kLuckyDigit = 7;
constexpr u16 kLuckyStep = 2;
constexpr int kTens = 10;
constexpr int kHundreds = 100;
constexpr int kThousands = 1000;
constexpr std::u8string_view kExperienceBonus = u8"此場戰鬥經驗值加百分之%d";   // 0x004BF038
constexpr std::u8string_view kMoneyTimes = u8"此場戰鬥金錢變%d倍";              // 0x004BF024
constexpr std::u8string_view kTreasureExperience = u8"龍颿法寶加%d點額外經驗";  // 0x004BF00C
}  // namespace swd3e
